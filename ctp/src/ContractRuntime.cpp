#include "ors/ctp/ContractRuntime.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

#include "ors/ctp/AeronCommandSubscriber.h"
#include "ors/ctp/AeronPublisher.h"
#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/CtpSession.h"

namespace ors::ctp {
namespace {
std::atomic_bool stopRequested{false};

void requestStop(int) {
  stopRequested.store(true, std::memory_order_relaxed);
}

std::uint64_t nowMillis() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count());
}
}  // namespace

struct ContractRuntime::Impl final : ExecutionSink, SnapshotSink {
  CtpConfig config;
  ContractRuntimeConfig runtimeConfig;
  CtpTraderSession trader;
  CtpAdapter adapter;
  AeronFramePublisher publisher;
  ContractSink sink;
  CtpMdSession md;
  CtpQueryService queries;
  AeronCommandSubscriber commands;
  std::size_t lastMdCount{0};
  bool mdPublished{false};
  std::chrono::steady_clock::time_point lastMdAt{};
  std::mutex sinkMutex;
  std::atomic_bool stopping{false};

  Impl(CtpConfig configValue, ContractRuntimeConfig runtimeValue)
      : config(std::move(configValue)),
        runtimeConfig(std::move(runtimeValue)),
        trader(config),
        adapter(trader),
        publisher(runtimeConfig.aeronDir, "aeron:ipc",
                  {runtimeConfig.streams.execution, runtimeConfig.streams.position,
                   runtimeConfig.streams.account, runtimeConfig.streams.session,
                   runtimeConfig.streams.marketData}),
        sink(publisher, ContractContext{
            .venueId = runtimeConfig.venueId,
            .accountId = config.investorId,
            .orsId = runtimeConfig.orsId,
            .environment = runtimeConfig.environment,
            .sourceSessionId = 1,
            .sourcePublicationEpoch = 1,
            .streams = runtimeConfig.streams}),
        md(config),
        queries(trader),
        commands(runtimeConfig.aeronDir, "aeron:ipc",
                 runtimeConfig.orderStream, runtimeConfig.cancelStream) {
    adapter.setSink(this);
    queries.setSink(this);
  }

  void onOrderUpdate(const OrderUpdate& update) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishOrderUpdate(update);
  }

  void onTrade(const Trade& trade) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishTrade(trade);
  }

  void onSnapshot(const AccountSnapshot& snapshot) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishAccountSnapshot(snapshot);
  }

  void onSnapshot(const std::vector<PositionSnapshot>& snapshot) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    for (const auto& position : snapshot) sink.publishPositionSnapshot(position);
  }

  void onSnapshot(const ReconciliationSnapshot& snapshot) override {
    (void)snapshot;
  }

  void publishSession() {
    const bool ready = trader.state() == SessionState::Ready;
    VenueSessionEvent session;
    session.tradingDay = trader.tradingDay();
    session.loginState = ready ? csbe::GatewayState::UP : csbe::GatewayState::CONNECTING;
    session.settlementState = ready ? csbe::SettlementState::SETTLED
                                    : csbe::SettlementState::NONE;
    session.adapterSessionId = runtimeConfig.orsId;
    session.state = ready ? csbe::VenueSessionState::READY
                          : csbe::VenueSessionState::STARTING;
    session.reason = ready ? "" : trader.lastError();
    session.snapshotTimestamp = nowMillis();
    session.lastSeqByStream = {
        {static_cast<std::uint32_t>(runtimeConfig.streams.execution), adapter.sequence()},
        {static_cast<std::uint32_t>(runtimeConfig.streams.marketData), 0},
    };
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishSession(session);
  }

  int run() {
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    if (!trader.start(std::chrono::seconds(120))) {
      std::cerr << "runtime trader start failed: " << trader.lastError() << '\n';
      return 1;
    }
    if (config.mdFront.empty()) {
      std::cerr << "runtime requires MD front\n";
      return 1;
    }
    if (!md.start(std::chrono::seconds(120))) {
      std::cerr << "runtime md start failed: " << md.lastError() << '\n';
      return 1;
    }
    if (!runtimeConfig.instruments.empty() && !md.subscribe(runtimeConfig.instruments)) {
      std::cerr << "runtime md subscribe failed: " << md.lastError() << '\n';
      return 1;
    }

    std::cerr << "runtime READY venue=" << runtimeConfig.venueId
              << " account=" << config.investorId
              << " ors=" << runtimeConfig.orsId
              << " streams=" << runtimeConfig.streams.execution
              << "," << runtimeConfig.streams.marketData << '\n';
    publishSession();
    std::thread queryThread([this] {
      while (!stopping.load(std::memory_order_relaxed)
             && !stopRequested.load(std::memory_order_relaxed)) {
        if (trader.state() == SessionState::Ready) {
          queries.queryAccount();
          queries.queryPositions();
        }
        std::this_thread::sleep_for(std::chrono::seconds(runtimeConfig.queryIntervalSeconds));
      }
    });
    auto lastSessionAt = std::chrono::steady_clock::now();
    while (!stopRequested.load(std::memory_order_relaxed)) {
      commands.poll(10,
          [this](const OrderRequest& request) {
            if (!adapter.onOrderRequest(request)) {
              std::cerr << "runtime order rejected: " << adapter.lastError() << '\n';
            }
          },
          [this](const CancelRequest& request) {
            if (!adapter.onCancelRequest(request)) {
              std::cerr << "runtime cancel rejected: " << adapter.lastError() << '\n';
            }
          });

      const std::size_t count = md.marketDataCount();
      if (count != lastMdCount) {
        lastMdCount = count;
        mdPublished = false;
      }
      const auto mdNow = std::chrono::steady_clock::now();
      if (lastMdCount > 0
          && (!mdPublished || mdNow - lastMdAt >= std::chrono::seconds(1))) {
        MarketData data = md.lastMarketData();
        if (data.exchange.empty()) data.exchange = runtimeConfig.exchange;
        std::lock_guard<std::mutex> lock(sinkMutex);
        mdPublished = sink.publishMarketData(data);
        lastMdAt = mdNow;
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - lastSessionAt >= std::chrono::seconds(1)) {
        lastSessionAt = now;
        publishSession();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stopping.store(true, std::memory_order_relaxed);
    if (queryThread.joinable()) queryThread.join();
    return 0;
  }
};

ContractRuntime::ContractRuntime(CtpConfig config, ContractRuntimeConfig runtimeConfig)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(runtimeConfig))) {}

ContractRuntime::~ContractRuntime() = default;

int ContractRuntime::run() {
  return impl_->run();
}

}  // namespace ors::ctp

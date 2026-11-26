#include "ors/ctp/ContractRuntime.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

#include "ors/ctp/AeronCommandSubscriber.h"
#include "ors/ctp/AeronPublisher.h"
#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/CtpSession.h"
#include "ors/ctp/SessionRecovery.h"
#include "ors/base/Log.h"
#include "ors/base/Metrics.h"

namespace ors::ctp {
namespace {
std::atomic_bool stopRequested{false};

void requestStop(int) {
  stopRequested.store(true, std::memory_order_relaxed);
}

std::uint64_t nowNanos() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count());
}

}  // namespace

struct ContractRuntime::Impl final : ExecutionSink, SnapshotSink {
  CtpConfig config;
  ContractRuntimeConfig runtimeConfig;
  std::shared_ptr<spdlog::logger> logger;
  base::Metrics metrics;
  CtpTraderSession trader;
  CtpAdapter adapter;
  AeronFramePublisher publisher;
  ContractSink sink;
  CtpMdSession md;
  CtpQueryService queries;
  AeronCommandSubscriber commands;
  std::size_t lastMdCount{0};
  bool mdPublished{false};
  bool mdStale{false};
  std::chrono::steady_clock::time_point lastMetricsAt{};
  std::mutex sinkMutex;
  std::atomic_bool stopping{false};
  std::atomic_bool transportHealthy{true};
  std::atomic<std::int64_t> transportRetryAtMillis{0};
  std::atomic<std::uint64_t> currentEpoch{1};
  SessionState lastTraderState{SessionState::Disconnected};

  Impl(CtpConfig configValue, ContractRuntimeConfig runtimeValue)
      : config(std::move(configValue)),
        runtimeConfig(std::move(runtimeValue)),
        logger(base::createLogger("ors", runtimeConfig.logFile)),
        trader(config, logger),
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
            .sourceSessionId = runtimeConfig.sourceSessionId,
            .sourcePublicationEpoch = runtimeConfig.sourcePublicationEpoch,
            .streams = runtimeConfig.streams}),
        md(config, logger),
        queries(trader),
        commands(runtimeConfig.aeronDir, "aeron:ipc",
                 runtimeConfig.orderStream, runtimeConfig.cancelStream) {
    currentEpoch.store(runtimeConfig.sourcePublicationEpoch);
    lastMetricsAt = std::chrono::steady_clock::now();
    adapter.setSink(this);
    queries.setSink(this);
  }

  void emit(spdlog::level::level_enum level, std::string_view event,
            std::initializer_list<base::LogField> fields) {
    base::logEvent(*logger, level, event, fields);
  }

  static std::int64_t steadyMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  void markTransportFailure(std::string_view frameType, std::uint64_t orderRef) {
    transportHealthy.store(false, std::memory_order_relaxed);
    transportRetryAtMillis.store(steadyMillis() + 1000, std::memory_order_relaxed);
    metrics.aeronFailed.fetch_add(1, std::memory_order_relaxed);
    emit(spdlog::level::err, "publish.fail",
         {base::LogField("frame_type", std::string(frameType)),
          base::LogField("order_ref", orderRef),
          base::LogField("stream_id", runtimeConfig.streams.execution)});
  }

  void onOrderUpdate(const OrderUpdate& update) override {
    OrderUpdate copy = update;
    copy.sourceSessionId = runtimeConfig.sourceSessionId;
    copy.sourcePublicationEpoch = currentEpoch.load(std::memory_order_relaxed);
    bool published = false;
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      published = sink.publishOrderUpdate(copy);
    }
    if (!published) markTransportFailure("order_update", update.orderRef);
  }

  void onTrade(const Trade& trade) override {
    Trade copy = trade;
    copy.sourceSessionId = runtimeConfig.sourceSessionId;
    copy.sourcePublicationEpoch = currentEpoch.load(std::memory_order_relaxed);
    bool published = false;
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      published = sink.publishTrade(copy);
    }
    if (!published) markTransportFailure("trade", trade.orderRef);
  }

  void onSnapshot(const AccountSnapshot& snapshot) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishAccountSnapshot(snapshot);
  }

  void onSnapshot(const std::vector<PositionSnapshot>& snapshot) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    for (const auto& position : snapshot) sink.publishPositionSnapshot(position);
  }

  void onSnapshot(const ReconciliationSnapshot&) override {
    // Order/trade queries are diagnostic only; runtime does not build order state.
  }

  void publishSession() {
    const SessionState traderState = trader.state();
    const bool ready = traderState == SessionState::Ready;
    const bool lost = traderState == SessionState::Disconnected ||
                      traderState == SessionState::Failed || mdStale;
    VenueSessionEvent session;
    session.tradingDay = trader.tradingDay();
    session.loginState = ready ? (mdStale ? csbe::GatewayState::DEGRADED
                                          : csbe::GatewayState::UP)
                               : csbe::GatewayState::DOWN;
    session.settlementState = ready ? csbe::SettlementState::SETTLED
                                    : csbe::SettlementState::NONE;
    session.adapterSessionId = runtimeConfig.orsId;
    session.state = ready ? (mdStale ? csbe::VenueSessionState::LOST
                                     : csbe::VenueSessionState::READY)
                          : (lost ? csbe::VenueSessionState::LOST
                                  : csbe::VenueSessionState::STARTING);
    session.reason = ready ? (mdStale ? "market data stale" : "")
                           : trader.lastError();
    session.snapshotTimestamp = nowNanos();
    session.lastSeqByStream = {
        {static_cast<std::uint32_t>(runtimeConfig.streams.execution), adapter.sequence()},
        {static_cast<std::uint32_t>(runtimeConfig.streams.marketData), 0},
    };
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishSession(session);
  }

  void beginReconnect() {
    const std::uint64_t oldEpoch = currentEpoch.load(std::memory_order_relaxed);
    const std::uint64_t epoch = currentEpoch.fetch_add(1) + 1;
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      sink.setSourcePublicationEpoch(epoch);
    }
    metrics.reconnects.fetch_add(1, std::memory_order_relaxed);
    emit(spdlog::level::info, "reconnect.epoch",
         {base::LogField("old_epoch", oldEpoch), base::LogField("new_epoch", epoch)});
    publishSession();
  }

  int run() {
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    if (!trader.start(std::chrono::seconds(300))) {
      emit(spdlog::level::err, "ors.start_failed",
           {base::LogField("reason", trader.lastError())});
      logger->flush();
      return 1;
    }
    if (config.mdFront.empty()) {
      emit(spdlog::level::err, "ors.start_failed",
           {base::LogField("reason", "MD front is empty")});
      logger->flush();
      return 1;
    }
    if (!md.start(std::chrono::seconds(120))) {
      emit(spdlog::level::err, "ors.start_failed",
           {base::LogField("reason", md.lastError())});
      logger->flush();
      return 1;
    }
    if (!runtimeConfig.instruments.empty() && !md.subscribe(runtimeConfig.instruments)) {
      emit(spdlog::level::err, "ors.start_failed",
           {base::LogField("reason", md.lastError())});
      logger->flush();
      return 1;
    }

    emit(spdlog::level::info, "ors.start",
         {base::LogField("venue_id", runtimeConfig.venueId),
          base::LogField("account_id", config.investorId),
          base::LogField("ors_id", runtimeConfig.orsId),
          base::LogField("environment", runtimeConfig.environment),
          base::LogField("source_session_id", runtimeConfig.sourceSessionId),
          base::LogField("source_publication_epoch", runtimeConfig.sourcePublicationEpoch),
          base::LogField("stream_execution", runtimeConfig.streams.execution),
          base::LogField("stream_market_data", runtimeConfig.streams.marketData)});
    publishSession();
    lastTraderState = trader.state();
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
    int exitCode = 0;
    while (!stopRequested.load(std::memory_order_relaxed)) {
      const SessionState traderState = trader.state();
      if (isTerminalSessionFailure(traderState)) {
        emit(spdlog::level::err, "ors.failed",
             {base::LogField("reason", trader.lastError())});
        exitCode = 1;
        break;
      }
      if (traderState != lastTraderState) {
        if (traderState != SessionState::Ready &&
            lastTraderState == SessionState::Ready) {
          beginReconnect();
        } else if (traderState == SessionState::Ready) {
          emit(spdlog::level::info, "reconnect.ready",
               {base::LogField("epoch", currentEpoch.load(std::memory_order_relaxed))});
          publishSession();
        }
        lastTraderState = traderState;
      }

      if (!transportHealthy.load(std::memory_order_relaxed) &&
          steadyMillis() >= transportRetryAtMillis.load(std::memory_order_relaxed) &&
          publisher.isConnected(runtimeConfig.streams.execution)) {
        transportHealthy.store(true, std::memory_order_relaxed);
        emit(spdlog::level::info, "transport.recovered", {});
      }

      commands.poll(10,
          [this](const OrderRequest& request) {
            metrics.ordersReceived.fetch_add(1, std::memory_order_relaxed);
            if (!transportHealthy.load(std::memory_order_relaxed)) {
              emit(spdlog::level::warn, "order.rejected",
                   {base::LogField("order_ref", request.orderRef),
                    base::LogField("reason", "transport_unhealthy")});
              return;
            }
            if (!adapter.onOrderRequest(request)) {
              emit(spdlog::level::warn, "order.rejected",
                   {base::LogField("order_ref", request.orderRef),
                    base::LogField("reason", adapter.lastError())});
            }
          },
          [this](const CancelRequest& request) {
            metrics.ordersReceived.fetch_add(1, std::memory_order_relaxed);
            if (!transportHealthy.load(std::memory_order_relaxed)) {
              emit(spdlog::level::warn, "cancel.rejected",
                   {base::LogField("order_ref", request.orderRef),
                    base::LogField("reason", "transport_unhealthy")});
              return;
            }
            if (!adapter.onCancelRequest(request)) {
              emit(spdlog::level::warn, "cancel.rejected",
                   {base::LogField("order_ref", request.orderRef),
                    base::LogField("reason", adapter.lastError())});
            }
          });

      const auto mdNow = std::chrono::steady_clock::now();
      const std::size_t count = md.marketDataCount();
      if (count != lastMdCount) {
        metrics.mdTicks.fetch_add(count - lastMdCount, std::memory_order_relaxed);
        lastMdCount = count;
        mdPublished = false;
      }
      const bool stale =
          lastMdCount > 0 &&
          md.isMarketDataStale(mdNow, std::chrono::milliseconds(5000));
      if (stale != mdStale) {
        mdStale = stale;
        metrics.mdStale.fetch_add(1, std::memory_order_relaxed);
        emit(stale ? spdlog::level::warn : spdlog::level::info,
             stale ? "md.stale" : "md.recovered", {});
        publishSession();
      }
      if (!mdStale && lastMdCount > 0 && !mdPublished) {
        MarketData data = md.lastMarketData();
        if (data.exchange.empty()) data.exchange = runtimeConfig.exchange;
        std::lock_guard<std::mutex> lock(sinkMutex);
        mdPublished = sink.publishMarketData(data);
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - lastSessionAt >= std::chrono::seconds(1)) {
        lastSessionAt = now;
        publishSession();
      }
      if (now - lastMetricsAt >= std::chrono::seconds(60)) {
        lastMetricsAt = now;
        base::logMetrics(*logger, metrics);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stopping.store(true, std::memory_order_relaxed);
    if (queryThread.joinable()) queryThread.join();
    emit(spdlog::level::info, "ors.shutdown", {});
    base::logMetrics(*logger, metrics);
    logger->flush();
    return exitCode;
  }
};

ContractRuntime::ContractRuntime(CtpConfig config, ContractRuntimeConfig runtimeConfig) {
  if (runtimeConfig.sourceSessionId == 0) runtimeConfig.sourceSessionId = nowNanos();
  if (runtimeConfig.sourcePublicationEpoch == 0) runtimeConfig.sourcePublicationEpoch = 1;
  impl_ = std::make_unique<Impl>(std::move(config), std::move(runtimeConfig));
}

ContractRuntime::~ContractRuntime() = default;

int ContractRuntime::run() {
  return impl_->run();
}

}  // namespace ors::ctp

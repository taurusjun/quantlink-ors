#include "ors/ctp/ContractRuntime.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "ors/ctp/AeronCommandSubscriber.h"
#include "ors/ctp/AeronPublisher.h"
#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/CtpReconciler.h"
#include "ors/ctp/CtpSession.h"

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

std::string trim(std::string_view value) {
  auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
  while (!value.empty() && isSpace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && isSpace(value.back())) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

std::string get(const std::unordered_map<std::string, std::string>& values,
                const std::string& key) {
  const auto found = values.find(key);
  return found == values.end() ? std::string() : found->second;
}

std::string require(const std::unordered_map<std::string, std::string>& values,
                    const std::string& key) {
  const std::string value = get(values, key);
  if (value.empty()) {
    throw std::runtime_error("ors runtime config: " + key + " is required");
  }
  return value;
}

std::uint64_t requireUnsigned(const std::unordered_map<std::string, std::string>& values,
                              const std::string& key) {
  const std::string text = require(values, key);
  std::size_t used = 0;
  unsigned long long value = 0;
  try {
    value = std::stoull(text, &used);
  } catch (const std::exception&) {
    throw std::runtime_error("ors runtime config: " + key + " must be an unsigned integer");
  }
  if (used != text.size()) {
    throw std::runtime_error("ors runtime config: " + key + " must be an unsigned integer");
  }
  return static_cast<std::uint64_t>(value);
}

int requirePositiveInt(const std::unordered_map<std::string, std::string>& values,
                       const std::string& key) {
  const std::string text = require(values, key);
  std::size_t used = 0;
  int value = 0;
  try {
    value = std::stoi(text, &used);
  } catch (const std::exception&) {
    throw std::runtime_error("ors runtime config: " + key + " must be a positive integer");
  }
  if (used != text.size() || value <= 0) {
    throw std::runtime_error("ors runtime config: " + key + " must be a positive integer");
  }
  return value;
}

std::vector<std::string> splitList(const std::string& value) {
  std::vector<std::string> items;
  std::string current;
  for (const char character : value) {
    if (character == ',' || character == ' ') {
      if (!current.empty()) {
        items.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(character);
    }
  }
  if (!current.empty()) {
    items.push_back(current);
  }
  return items;
}
}  // namespace

ContractRuntimeConfig ContractRuntimeConfig::load(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("ors runtime config: cannot open " + path);
  }

  std::unordered_map<std::string, std::string> values;
  std::string line;
  while (std::getline(in, line)) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) {
      line = line.substr(0, hash);
    }
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = trim(std::string_view(line).substr(0, equals));
    if (!key.empty()) {
      values[key] = trim(std::string_view(line).substr(equals + 1));
    }
  }

  ContractRuntimeConfig config;
  config.aeronDir = require(values, "AERON_DIR");
  config.venueId = require(values, "VENUE_ID");
  config.orsId = require(values, "ORS_ID");
  config.environment = require(values, "ENVIRONMENT");
  config.exchange = require(values, "EXCHANGE");
  config.instruments = splitList(require(values, "INSTRUMENTS"));
  if (config.instruments.empty()) {
    throw std::runtime_error("ors runtime config: INSTRUMENTS is required");
  }
  config.sourcePublicationEpoch = requireUnsigned(values, "SOURCE_PUBLICATION_EPOCH");
  if (values.find("SOURCE_SESSION_ID") != values.end()) {
    config.sourceSessionId = requireUnsigned(values, "SOURCE_SESSION_ID");
  }

  const int streamBase = requirePositiveInt(values, "STREAM_BASE");
  if (streamBase > std::numeric_limits<int>::max() - 7) {
    throw std::runtime_error("ors runtime config: STREAM_BASE is too large");
  }
  config.orderStream = streamBase + 2;
  config.cancelStream = streamBase + 3;
  config.streams = {streamBase + 4, streamBase + 5, streamBase + 6,
                    streamBase + 7, streamBase + 1};

  if (values.find("QUERY_INTERVAL_SECONDS") != values.end()) {
    config.queryIntervalSeconds = requirePositiveInt(values, "QUERY_INTERVAL_SECONDS");
  }
  return config;
}

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
  CtpReconciler reconciler;
  std::size_t lastMdCount{0};
  bool mdPublished{false};
  std::chrono::steady_clock::time_point lastMdAt{};
  std::mutex sinkMutex;
  std::mutex reconMutex;
  std::atomic_bool stopping{false};
  std::atomic_bool reconciling{false};
  std::atomic<std::uint64_t> currentEpoch{1};
  SessionState lastTraderState{SessionState::Disconnected};
  std::vector<OrderSnapshot> reconOrders;
  std::vector<TradeSnapshot> reconTrades;
  std::vector<PositionSnapshot> reconPositions;
  bool reconOrdersReady{false};
  bool reconTradesReady{false};
  bool reconPositionsReady{false};

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
            .sourceSessionId = runtimeConfig.sourceSessionId,
            .sourcePublicationEpoch = runtimeConfig.sourcePublicationEpoch,
            .streams = runtimeConfig.streams}),
        md(config),
        queries(trader),
        commands(runtimeConfig.aeronDir, "aeron:ipc",
                 runtimeConfig.orderStream, runtimeConfig.cancelStream) {
    currentEpoch.store(runtimeConfig.sourcePublicationEpoch);
    reconciler.setEpoch(runtimeConfig.sourcePublicationEpoch);
    adapter.setSink(this);
    queries.setSink(this);
  }

  void onOrderUpdate(const OrderUpdate& update) override {
    OrderUpdate copy = update;
    copy.sourceSessionId = runtimeConfig.sourceSessionId;
    copy.sourcePublicationEpoch = currentEpoch.load(std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      sink.publishOrderUpdate(copy);
    }
    std::lock_guard<std::mutex> lock(reconMutex);
    reconciler.observe(copy);
  }

  void onTrade(const Trade& trade) override {
    Trade copy = trade;
    copy.sourceSessionId = runtimeConfig.sourceSessionId;
    copy.sourcePublicationEpoch = currentEpoch.load(std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      sink.publishTrade(copy);
    }
    std::lock_guard<std::mutex> lock(reconMutex);
    reconciler.observe(copy);
  }

  void onSnapshot(const AccountSnapshot& snapshot) override {
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishAccountSnapshot(snapshot);
  }

  void onSnapshot(const std::vector<PositionSnapshot>& snapshot) override {
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      for (const auto& position : snapshot) sink.publishPositionSnapshot(position);
    }
    if (!reconciling.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lock(reconMutex);
    reconPositions = snapshot;
    reconPositionsReady = true;
  }

  void onSnapshot(const ReconciliationSnapshot& snapshot) override {
    std::lock_guard<std::mutex> lock(reconMutex);
    if (!snapshot.orders.empty()) {
      reconOrders = snapshot.orders;
    }
    if (!snapshot.trades.empty()) {
      reconTrades = snapshot.trades;
    }
  }

  void publishSession() {
    const bool traderReady = trader.state() == SessionState::Ready;
    const bool reconnecting = reconciling.load(std::memory_order_relaxed);
    const bool ready = traderReady && !reconnecting;
    VenueSessionEvent session;
    session.tradingDay = trader.tradingDay();
    session.loginState = traderReady ? csbe::GatewayState::UP : csbe::GatewayState::DOWN;
    session.settlementState = traderReady ? csbe::SettlementState::SETTLED
                                          : csbe::SettlementState::NONE;
    session.adapterSessionId = runtimeConfig.orsId;
    session.state = reconnecting ? csbe::VenueSessionState::LOST
                                 : (ready ? csbe::VenueSessionState::READY
                                          : csbe::VenueSessionState::STARTING);
    session.reason = reconnecting ? "reconnect reconciliation pending"
                                  : (ready ? "" : trader.lastError());
    session.snapshotTimestamp = nowNanos();
    session.lastSeqByStream = {
        {static_cast<std::uint32_t>(runtimeConfig.streams.execution), adapter.sequence()},
        {static_cast<std::uint32_t>(runtimeConfig.streams.marketData), 0},
    };
    std::lock_guard<std::mutex> lock(sinkMutex);
    sink.publishSession(session);
  }

  void beginReconnect() {
    if (reconciling.exchange(true)) return;
    const std::uint64_t epoch = currentEpoch.fetch_add(1) + 1;
    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      sink.setSourcePublicationEpoch(epoch);
    }
    {
      std::lock_guard<std::mutex> lock(reconMutex);
      reconciler.onReconnect();
      reconOrders.clear();
      reconTrades.clear();
      reconPositions.clear();
      reconOrdersReady = false;
      reconTradesReady = false;
      reconPositionsReady = false;
    }
    std::cerr << "runtime reconnecting epoch=" << epoch << '\n';
    publishSession();
  }

  bool finishReconciliation() {
    std::vector<OrderSnapshot> orders;
    std::vector<TradeSnapshot> trades;
    std::vector<PositionSnapshot> positions;
    {
      std::lock_guard<std::mutex> lock(reconMutex);
      if (!reconOrdersReady || !reconTradesReady || !reconPositionsReady) return false;
      orders = reconOrders;
      trades = reconTrades;
      positions = reconPositions;
      reconciler.reconcile(orders, trades, positions);
      reconOrdersReady = false;
      reconTradesReady = false;
      reconPositionsReady = false;
    }
    reconciling.store(false, std::memory_order_relaxed);
    std::cerr << "runtime reconciliation complete epoch="
              << currentEpoch.load(std::memory_order_relaxed) << '\n';
    publishSession();
    return true;
  }

  void queryReconciliation() {
    {
      std::lock_guard<std::mutex> lock(reconMutex);
      reconOrders.clear();
      reconTrades.clear();
      reconPositions.clear();
      reconOrdersReady = false;
      reconTradesReady = false;
      reconPositionsReady = false;
    }
    const bool ordersOk = queries.queryOrders();
    const bool tradesOk = queries.queryTrades();
    const bool positionsOk = queries.queryPositions();
    {
      std::lock_guard<std::mutex> lock(reconMutex);
      reconOrdersReady = ordersOk;
      reconTradesReady = tradesOk;
      reconPositionsReady = positionsOk;
    }
    if (!ordersOk || !tradesOk || !positionsOk) {
      std::cerr << "runtime reconciliation query failed: " << queries.lastError() << '\n';
      return;
    }
    finishReconciliation();
  }

  int run() {
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    if (!trader.start(std::chrono::seconds(300))) {
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
              << " sourceSession=" << runtimeConfig.sourceSessionId
              << " epoch=" << runtimeConfig.sourcePublicationEpoch
              << " streams=" << runtimeConfig.streams.execution
              << "," << runtimeConfig.streams.marketData << '\n';
    publishSession();
    lastTraderState = trader.state();
    std::thread queryThread([this] {
      while (!stopping.load(std::memory_order_relaxed)
             && !stopRequested.load(std::memory_order_relaxed)) {
        if (trader.state() == SessionState::Ready) {
          if (reconciling.load(std::memory_order_relaxed)) {
            queryReconciliation();
          } else {
            queries.queryAccount();
            queries.queryPositions();
          }
        }
        std::this_thread::sleep_for(std::chrono::seconds(runtimeConfig.queryIntervalSeconds));
      }
    });
    auto lastSessionAt = std::chrono::steady_clock::now();
    while (!stopRequested.load(std::memory_order_relaxed)) {
      const SessionState traderState = trader.state();
      if (traderState != lastTraderState) {
        if (!reconciling.load(std::memory_order_relaxed)
            && traderState != SessionState::Ready
            && lastTraderState == SessionState::Ready) {
          beginReconnect();
        }
        lastTraderState = traderState;
      }

      commands.poll(10,
          [this](const OrderRequest& request) {
            if (reconciling.load(std::memory_order_relaxed)) {
              adapter.rejectOrderRequest(request, "reconnect reconciliation pending");
              return;
            }
            bool allowed = false;
            {
              std::lock_guard<std::mutex> lock(reconMutex);
              allowed = reconciler.allowsNewOrder(reconciling.load(std::memory_order_relaxed),
                                                    request.orderRef);
            }
            if (!allowed) {
              adapter.rejectOrderRequest(request, "orderRef reconciliation pending");
              return;
            }
            if (!adapter.onOrderRequest(request)) {
              std::cerr << "runtime order rejected: " << adapter.lastError() << '\n';
            }
          },
          [this](const CancelRequest& request) {
            if (reconciling.load(std::memory_order_relaxed)) {
              adapter.rejectCancelRequest(request, "reconnect reconciliation pending");
              return;
            }
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

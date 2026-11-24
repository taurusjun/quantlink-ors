#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/CtpSession.h"
#include "ors/ctp/ContractRuntime.h"

namespace {

using ors::ctp::CancelRequest;
using ors::ctp::CancelState;
using ors::ctp::CtpAdapter;
using ors::ctp::CtpConfig;
using ors::ctp::CtpTraderSession;
using ors::ctp::ExecutionSink;
using ors::ctp::Offset;
using ors::ctp::OrderRequest;
using ors::ctp::OrderStatus;
using ors::ctp::OrderType;
using ors::ctp::OrderUpdate;
using ors::ctp::Side;
using ors::ctp::TimeInForce;
using ors::ctp::Trade;

void usage() {
  std::cout << "usage: ors_ctp [--config PATH] [--mode login-smoke|md-smoke|query-smoke|order-smoke|runtime]\n"
               "              [--instruments ID[,ID...]]\n"
               "              [--price PRICE --qty N [--side buy|sell]\n"
               "               [--offset open|close|close-today|close-yesterday] [--live]]\n"
               "\n"
               "  (no arguments)          print this usage and exit\n"
               "  --mode login-smoke      log in to CTP, confirm settlement, query\n"
               "                          instruments, print READY, exit 0\n"
               "  --mode md-smoke         log in to MdApi, subscribe --instruments,\n"
               "                          print the first tick padded to 20 levels,\n"
               "                          exit 0\n"
               "  --mode query-smoke      log in to CTP and print the account,\n"
               "                          position, order and trade snapshots\n"
               "  --mode order-smoke      send one limit order on the single\n"
               "                          --instruments entry, cancel it once the\n"
               "                          venue acknowledged it, print every\n"
               "                          OrderUpdate/Trade. Dry-run unless --live\n";
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

/// One "price@qty" per level, so the smoke output shows where CTP data stops.
template <typename Prices, typename Quantities>
std::string formatLevels(const Prices& prices, const Quantities& qtys) {
  std::ostringstream out;
  for (std::size_t level = 0; level < prices.size(); ++level) {
    if (level != 0) {
      out << ' ';
    }
    out << prices[level] << '@' << qtys[level];
  }
  return out.str();
}

int loginSmoke(const CtpConfig& config) {
  ors::ctp::CtpTraderSession session(config);
  if (!session.start(std::chrono::seconds(120))) {
    std::cerr << "FAILED: " << session.lastError() << '\n';
    return 1;
  }
  std::cout << toString(session.state()) << " instruments="
            << session.instruments().size() << '\n';
  return 0;
}

/// Prints each finished snapshot; the sink runs on the querying thread.
struct SnapshotPrinter final : ors::ctp::SnapshotSink {
  void onSnapshot(const ors::ctp::AccountSnapshot& snapshot) override {
    const auto& header = snapshot.header;
    std::cout << "account snapshot=" << header.snapshotId << " tradingDay=" << header.tradingDay
              << " account=" << header.accountId << " balance=" << snapshot.balance
              << " available=" << snapshot.available << " margin=" << snapshot.margin
              << " frozenMargin=" << snapshot.frozenMargin
              << " commission=" << snapshot.commission
              << " closeProfit=" << snapshot.closeProfit
              << " positionProfit=" << snapshot.positionProfit
              << " deposit=" << snapshot.deposit << " withdraw=" << snapshot.withdraw << '\n';
  }

  void onSnapshot(const std::vector<ors::ctp::PositionSnapshot>& snapshot) override {
    std::cout << "positions rows=" << snapshot.size();
    if (!snapshot.empty()) {
      std::cout << " snapshot=" << snapshot.front().header.snapshotId
                << " tradingDay=" << snapshot.front().header.tradingDay;
    }
    std::cout << '\n';
    for (const auto& row : snapshot) {
      std::cout << "  position " << row.instrumentId << ' ' << row.exchange
                << " long=" << row.longQty << " short=" << row.shortQty
                << " todayLong=" << row.todayLong << " todayShort=" << row.todayShort
                << " yesterdayLong=" << row.yesterdayLong
                << " yesterdayShort=" << row.yesterdayShort << " avg=" << row.avgPrice
                << " longAvg=" << row.longAvgPrice << " shortAvg=" << row.shortAvgPrice
                << " settlement=" << row.settlementPrice << '\n';
    }
  }

  void onSnapshot(const ors::ctp::ReconciliationSnapshot& snapshot) override {
    std::cout << "orders rows=" << snapshot.orders.size()
              << " trades rows=" << snapshot.trades.size();
    if (!snapshot.orders.empty()) {
      std::cout << " ordersSnapshot=" << snapshot.orders.front().header.snapshotId;
    }
    if (!snapshot.trades.empty()) {
      std::cout << " tradesSnapshot=" << snapshot.trades.front().header.snapshotId;
    }
    std::cout << '\n';
    for (const auto& row : snapshot.orders) {
      std::cout << "  order " << row.instrumentId << " ref=" << row.orderRef
                << " sysId=" << row.orderSysId << " status=" << row.orderStatus
                << " traded=" << row.volumeTraded << '/' << row.volumeTotal
                << " original=" << row.volumeTotalOriginal
                << " front=" << row.frontId << " session=" << row.sessionId
                << " limit=" << row.limitPrice << '\n';
    }
    for (const auto& row : snapshot.trades) {
      std::cout << "  trade " << row.instrumentId << " id=" << row.tradeId
                << " ref=" << row.orderRef << " price=" << row.price
                << " volume=" << row.volume << " at=" << row.tradeDate << ' '
                << row.tradeTime << '\n';
    }
  }
};

int querySmoke(const CtpConfig& config) {
  ors::ctp::CtpTraderSession session(config);
  if (!session.start(std::chrono::seconds(120))) {
    std::cerr << "FAILED: " << session.lastError() << '\n';
    return 1;
  }
  std::cout << toString(session.state()) << " instruments="
            << session.instruments().size() << '\n';

  SnapshotPrinter printer;
  ors::ctp::CtpQueryService queries(session);
  queries.setSink(&printer);

  using Query = bool (ors::ctp::CtpQueryService::*)(std::chrono::milliseconds);
  const std::vector<std::pair<const char*, Query>> queriesToRun{
      {"account", &ors::ctp::CtpQueryService::queryAccount},
      {"positions", &ors::ctp::CtpQueryService::queryPositions},
      {"orders", &ors::ctp::CtpQueryService::queryOrders},
      {"trades", &ors::ctp::CtpQueryService::queryTrades},
  };
  int failed = 0;
  bool first = true;
  for (const auto& [name, query] : queriesToRun) {
    if (!first) {
      // CTP rate-limits trade queries to roughly one per second.
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    first = false;
    std::cout << "query " << name << '\n';
    if (!(queries.*query)(std::chrono::seconds(15))) {
      std::cerr << "FAILED: " << name << " query: " << queries.lastError() << '\n';
      ++failed;
    }
  }
  return failed == 0 ? 0 : 1;
}

int mdSmoke(const CtpConfig& config, const std::vector<std::string>& instruments) {
  ors::ctp::CtpMdSession session(config);
  if (!session.start(std::chrono::seconds(120))) {
    std::cerr << "FAILED: " << session.lastError() << '\n';
    return 1;
  }
  if (!session.subscribe(instruments)) {
    std::cerr << "FAILED: " << session.lastError() << '\n';
    return 1;
  }
  if (!session.waitForMarketData(std::chrono::seconds(15))) {
    std::cerr << "FAILED: no market data within 15s (" << session.lastError() << ")\n";
    return 1;
  }
  const ors::ctp::MarketData md = session.lastMarketData();
  std::cout << toString(session.state()) << " md ticks=" << session.marketDataCount()
            << " instrument=" << md.instrumentId << " exchange=" << md.exchange
            << " update=" << md.updateTime << '.' << md.updateMillisec
            << " last=" << md.lastPrice << " volume=" << md.volume
            << " levels=" << md.bidPrices.size() << '\n';
  std::cout << "bids " << formatLevels(md.bidPrices, md.bidQtys) << '\n';
  std::cout << "asks " << formatLevels(md.askPrices, md.askQtys) << '\n';
  return 0;
}

/// Prints execution events and lets the order smoke wait for the venue answer.
struct ExecutionPrinter final : ExecutionSink {
  mutable std::mutex mutex;
  std::condition_variable cv;
  int updates{0};
  int tradeCount{0};
  bool terminal{false};
  bool cancelOutcome{false};
  OrderStatus status{OrderStatus::New};
  CancelState cancelState{CancelState::None};

  void onOrderUpdate(const OrderUpdate& update) override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++updates;
      status = update.status;
      cancelState = update.cancelState;
      if (update.status == OrderStatus::Filled || update.status == OrderStatus::Canceled ||
          update.status == OrderStatus::Rejected) {
        terminal = true;
      }
      if (update.cancelState == CancelState::Rejected || update.status == OrderStatus::Canceled ||
          update.status == OrderStatus::Filled) {
        cancelOutcome = true;
      }
    }
    std::cout << "orderUpdate seq=" << update.sequence << " ref=" << update.orderRef
              << " status=" << toString(update.status)
              << " cancel=" << toString(update.cancelState) << " filled=" << update.filledQty
              << '/' << update.orderQty << " remaining=" << update.remainingQty
              << " avg=" << update.avgPrice << " price=" << update.price
              << " exchangeOrderId=" << update.exchangeOrderId
              << " error=" << update.errorCode << " reason=" << update.reason << '\n';
    cv.notify_all();
  }

  void onTrade(const Trade& trade) override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++tradeCount;
      cancelOutcome = true;
    }
    std::cout << "trade seq=" << trade.sequence << " ref=" << trade.orderRef
              << " id=" << trade.tradeId << " price=" << trade.price << " qty=" << trade.qty
              << '\n';
    cv.notify_all();
  }

  bool waitForUpdates(int count, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [this, count] { return updates >= count; });
  }

  bool waitForCancelOutcome(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [this] { return cancelOutcome; });
  }

  bool waitForTrade(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [this] { return tradeCount > 0; });
  }

  bool isTerminal() const {
    std::lock_guard<std::mutex> lock(mutex);
    return terminal;
  }

  OrderStatus lastStatus() const {
    std::lock_guard<std::mutex> lock(mutex);
    return status;
  }

  CancelState lastCancelState() const {
    std::lock_guard<std::mutex> lock(mutex);
    return cancelState;
  }

  int updateCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return updates;
  }

  int trades() const {
    std::lock_guard<std::mutex> lock(mutex);
    return tradeCount;
  }
};

bool parsePrice(const std::string& text, double& price) {
  std::size_t used = 0;
  try {
    price = std::stod(text, &used);
  } catch (const std::exception&) {
    return false;
  }
  return used == text.size() && std::isfinite(price);
}

bool parseQty(const std::string& text, int& qty) {
  std::size_t used = 0;
  try {
    qty = std::stoi(text, &used);
  } catch (const std::exception&) {
    return false;
  }
  return used == text.size();
}

/// order-smoke: one explicit limit order, one cancel. Dry-run unless --live.
int orderSmoke(const std::string& configPath, const std::vector<std::string>& instruments,
               const std::string& priceText, const std::string& qtyText,
               const std::string& sideText, const std::string& offsetText, bool send) {
  if (instruments.size() != 1) {
    std::cerr << "--instruments must name exactly one instrument for --mode order-smoke\n";
    return 2;
  }
  double price = 0.0;
  int qty = 0;
  if (!parsePrice(priceText, price) || price <= 0.0) {
    std::cerr << "--price must be a positive number for --mode order-smoke\n";
    return 2;
  }
  if (!parseQty(qtyText, qty) || qty <= 0) {
    std::cerr << "--qty must be a positive integer for --mode order-smoke\n";
    return 2;
  }
  Side side = Side::Buy;
  if (!sideText.empty() && sideText != "buy" && sideText != "sell") {
    std::cerr << "--side must be buy or sell\n";
    return 2;
  }
  side = sideText == "sell" ? Side::Sell : Side::Buy;
  Offset offsetFlag = Offset::Open;
  if (!offsetText.empty() && offsetText != "open" && offsetText != "close" &&
      offsetText != "close-today" && offsetText != "close-yesterday") {
    std::cerr << "--offset must be open, close, close-today or close-yesterday\n";
    return 2;
  }
  if (offsetText == "close") {
    offsetFlag = Offset::Close;
  } else if (offsetText == "close-today") {
    offsetFlag = Offset::CloseToday;
  } else if (offsetText == "close-yesterday") {
    offsetFlag = Offset::CloseYesterday;
  }

  const std::string& instrument = instruments.front();
  OrderRequest request;
  request.orderRef = 1;
  request.instrumentId = instrument;
  request.side = side;
  request.offsetFlag = offsetFlag;
  request.orderType = OrderType::Limit;
  request.timeInForce = TimeInForce::Day;
  request.price = price;
  request.qty = qty;
  request.requestId = "order-smoke-1";

  if (!send) {
    std::cout << "DRY-RUN: insert orderRef=" << request.orderRef << " instrument=" << instrument
              << " side=" << (sideText.empty() ? "buy" : sideText)
              << " offset=" << (offsetText.empty() ? "open" : offsetText)
              << " type=limit tif=day price=" << price << " qty=" << qty << '\n'
              << "DRY-RUN: then cancel orderRef=" << request.orderRef
              << " once the venue acknowledged it\n"
              << "DRY-RUN: nothing is connected and no order is sent (pass --live to trade)\n";
    return 0;
  }

  const CtpConfig config = CtpConfig::load(configPath);
  std::cout << "config: " << config.summary() << '\n';

  CtpTraderSession session(config);
  if (!session.start(std::chrono::seconds(120))) {
    std::cerr << "FAILED: " << session.lastError() << '\n';
    return 1;
  }
  std::cout << toString(session.state()) << " instruments=" << session.instruments().size()
            << '\n';

  ExecutionPrinter printer;
  CtpAdapter adapter(session);
  adapter.setSink(&printer);

  if (!adapter.onOrderRequest(request)) {
    std::cerr << "FAILED: insert orderRef=" << request.orderRef << ": " << adapter.lastError()
              << '\n';
    return 1;
  }
  std::cout << "insert sent orderRef=" << request.orderRef << '\n';

  // The NEW update is published by onOrderRequest itself, so the second one is
  // the venue's first report.
  if (!printer.waitForUpdates(2, std::chrono::seconds(10))) {
    std::cerr << "FAILED: no venue order report within 10s after the insert\n";
    return 1;
  }
  if (printer.isTerminal()) {
    const OrderStatus status = printer.lastStatus();
    if (status == OrderStatus::Filled && printer.trades() == 0) {
      printer.waitForTrade(std::chrono::seconds(2));
    }
    std::cout << "order is terminal (" << toString(status) << "); cancel skipped\n";
    std::cout << "RESULT updates=" << printer.updateCount() << " trades=" << printer.trades()
              << " lastStatus=" << toString(printer.lastStatus())
              << " cancelState=" << toString(printer.lastCancelState()) << '\n';
    return status == OrderStatus::Rejected ? 1 : 0;
  }

  CancelRequest cancel;
  cancel.orderRef = request.orderRef;
  cancel.instrumentId = instrument;
  cancel.requestId = "cancel-smoke-1";
  if (!adapter.onCancelRequest(cancel)) {
    std::cerr << "cancel orderRef=" << cancel.orderRef << " refused: " << adapter.lastError()
              << '\n';
    return printer.isTerminal() ? 0 : 1;
  }
  std::cout << "cancel sent orderRef=" << cancel.orderRef << '\n';

  if (!printer.waitForCancelOutcome(std::chrono::seconds(10))) {
    std::cerr << "FAILED: no cancel result or trade within 10s after the cancel\n";
    return 1;
  }
  if (printer.lastStatus() == OrderStatus::Filled && printer.trades() == 0) {
    printer.waitForTrade(std::chrono::seconds(2));
  }
  std::cout << "RESULT updates=" << printer.updateCount() << " trades=" << printer.trades()
            << " lastStatus=" << toString(printer.lastStatus())
            << " cancelState=" << toString(printer.lastCancelState()) << '\n';
  return printer.lastCancelState() == CancelState::Rejected ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string configPath;
  std::string mode;
  std::string instrumentList;
  std::string priceText;
  std::string qtyText;
  std::string sideText;
  std::string offsetText;
  bool live = false;
  bool dryRun = false;
  std::string aeronDir;
  std::string venueId = "SIMNOW";
  std::string orsId = "ors-ctp-1";
  std::string environment = "SHADOW";
  std::string exchange = "SHFE";
  int streamBase = 9000;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      configPath = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      mode = argv[++index];
    } else if (argument == "--instruments" && index + 1 < argc) {
      instrumentList = argv[++index];
    } else if (argument == "--price" && index + 1 < argc) {
      priceText = argv[++index];
    } else if (argument == "--qty" && index + 1 < argc) {
      qtyText = argv[++index];
    } else if (argument == "--side" && index + 1 < argc) {
      sideText = argv[++index];
    } else if (argument == "--offset" && index + 1 < argc) {
      offsetText = argv[++index];
    } else if (argument == "--live") {
      live = true;
    } else if (argument == "--dry-run") {
      dryRun = true;
    } else if (argument == "--aeron-dir" && index + 1 < argc) {
      aeronDir = argv[++index];
    } else if (argument == "--venue-id" && index + 1 < argc) {
      venueId = argv[++index];
    } else if (argument == "--ors-id" && index + 1 < argc) {
      orsId = argv[++index];
    } else if (argument == "--environment" && index + 1 < argc) {
      environment = argv[++index];
    } else if (argument == "--exchange" && index + 1 < argc) {
      exchange = argv[++index];
    } else if (argument == "--stream-base" && index + 1 < argc) {
      streamBase = std::stoi(argv[++index]);
    } else if (argument == "--help" || argument == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "unknown or incomplete argument: " << argument << '\n';
      usage();
      return 2;
    }
  }

  if (mode.empty()) {
    if (!configPath.empty()) {
      std::cerr << "--mode is required with --config\n";
      return 2;
    }
    usage();
    return 0;
  }
  if (live && dryRun) {
    std::cerr << "--live and --dry-run contradict each other\n";
    return 2;
  }
  if (mode != "login-smoke" && mode != "md-smoke" && mode != "query-smoke" &&
      mode != "order-smoke" && mode != "runtime") {
    std::cerr << "unsupported mode: " << mode << '\n';
    usage();
    return 2;
  }
  // A dry-run order-smoke never connects, so it needs no config.
  const bool sendOrders = live && !dryRun;
  if ((mode != "order-smoke" || sendOrders) && configPath.empty()) {
    std::cerr << "--config is required for --mode " << mode << '\n';
    return 2;
  }

  try {
    if (mode == "order-smoke") {
      return orderSmoke(configPath, splitList(instrumentList), priceText, qtyText, sideText,
                        offsetText, sendOrders);
    }

    const CtpConfig config = CtpConfig::load(configPath);
    std::cout << "config: " << config.summary() << '\n';

    if (mode == "runtime") {
      if (aeronDir.empty() || instrumentList.empty()) {
        std::cerr << "--aeron-dir and --instruments are required for --mode runtime\n";
        return 2;
      }
      ors::ctp::ContractRuntimeConfig runtimeConfig;
      runtimeConfig.aeronDir = aeronDir;
      runtimeConfig.venueId = venueId;
      runtimeConfig.orsId = orsId;
      runtimeConfig.environment = environment;
      runtimeConfig.exchange = exchange;
      runtimeConfig.instruments = splitList(instrumentList);
      runtimeConfig.orderStream = streamBase + 2;
      runtimeConfig.cancelStream = streamBase + 3;
      runtimeConfig.streams = {streamBase + 4, streamBase + 5, streamBase + 6,
                               streamBase + 7, streamBase + 1};
      ors::ctp::ContractRuntime runtime(config, std::move(runtimeConfig));
      return runtime.run();
    }

    if (mode == "login-smoke") {
      return loginSmoke(config);
    }
    if (mode == "query-smoke") {
      return querySmoke(config);
    }

    // Only md-smoke needs a market data front; a trader-only run stays
    // tolerant of a config without one.
    if (config.mdFront.empty()) {
      std::cerr << "config has no MD_FRONT (or FRONT_ADDR_MD), required for --mode "
                << mode << '\n';
      return 2;
    }
    const std::vector<std::string> instruments = splitList(instrumentList);
    if (instruments.empty()) {
      std::cerr << "--instruments is required for --mode " << mode << '\n';
      return 2;
    }
    return mdSmoke(config, instruments);
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
  }
}

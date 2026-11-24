#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/CtpSession.h"

namespace {

using ors::ctp::CtpConfig;

void usage() {
  std::cout << "usage: ors_ctp [--config PATH] [--mode login-smoke|md-smoke|query-smoke]\n"
               "              [--instruments ID[,ID...]]\n"
               "\n"
               "  (no arguments)          print this usage and exit\n"
               "  --mode login-smoke      log in to CTP, confirm settlement, query\n"
               "                          instruments, print READY, exit 0\n"
               "  --mode md-smoke         log in to MdApi, subscribe --instruments,\n"
               "                          print the first tick padded to 20 levels,\n"
               "                          exit 0\n"
               "  --mode query-smoke      log in to CTP and print the account,\n"
               "                          position, order and trade snapshots\n";
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
  if (!session.start()) {
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
  if (!session.start()) {
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
  if (!session.start()) {
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

}  // namespace

int main(int argc, char** argv) {
  std::string configPath;
  std::string mode;
  std::string instrumentList;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      configPath = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      mode = argv[++index];
    } else if (argument == "--instruments" && index + 1 < argc) {
      instrumentList = argv[++index];
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
  if (mode != "login-smoke" && mode != "md-smoke" && mode != "query-smoke") {
    std::cerr << "unsupported mode: " << mode << '\n';
    usage();
    return 2;
  }
  if (configPath.empty()) {
    std::cerr << "--config is required for --mode " << mode << '\n';
    return 2;
  }

  try {
    const CtpConfig config = CtpConfig::load(configPath);
    std::cout << "config: " << config.summary() << '\n';

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

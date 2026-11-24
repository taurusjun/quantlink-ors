#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ors/ctp/CtpConfig.h"
#include "ors/ctp/CtpSession.h"

namespace {

using ors::ctp::CtpConfig;

void usage() {
  std::cout << "usage: ors_ctp [--config PATH] [--mode login-smoke|md-smoke]\n"
               "              [--instruments ID[,ID...]]\n"
               "\n"
               "  (no arguments)          print this usage and exit\n"
               "  --mode login-smoke      log in to CTP, confirm settlement, query\n"
               "                          instruments, print READY, exit 0\n"
               "  --mode md-smoke         log in to MdApi, subscribe --instruments,\n"
               "                          print the first tick padded to 20 levels,\n"
               "                          exit 0\n";
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
  if (mode != "login-smoke" && mode != "md-smoke") {
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

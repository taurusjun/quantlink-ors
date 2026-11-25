// CtpMdSession mapping test: the 20-level contract shape from a 5-level CTP
// tick. No login and no network; only mapDepthMarketData() is exercised.

#include <cstddef>
#include <iostream>
#include <limits>
#include <string>

#include "ors/ctp/CtpSession.h"

namespace {

using ors::ctp::DepthTick;
using ors::ctp::kCtpDepthLevels;
using ors::ctp::kMarketDepthLevels;
using ors::ctp::MarketData;
using ors::ctp::mapDepthMarketData;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

DepthTick fakeTick() {
  DepthTick tick;
  tick.instrumentId = "rb2601";
  tick.exchangeId = "SHFE";
  tick.lastPrice = 3421.0;
  tick.lastVolume = 12345;
  tick.turnover = 4.2e8;
  tick.openInterest = 98765.0;
  tick.updateTime = "10:15:00";
  tick.updateMillisec = 500;
  tick.bidPrices = {3420.0, 3419.0, 3418.0, 3417.0, 3416.0};
  tick.bidQtys = {5, 4, 3, 2, 1};
  tick.askPrices = {3421.0, 3422.0, 3423.0, 3424.0, 3425.0};
  tick.askQtys = {6, 7, 8, 9, 10};
  return tick;
}

}  // namespace

int main() {
  const MarketData md = mapDepthMarketData(fakeTick());

  // Exactly 20 levels on each side, and only the first five are CTP's.
  check(md.bidPrices.size() == kMarketDepthLevels, "bidPrices has 20 levels");
  check(md.bidQtys.size() == kMarketDepthLevels, "bidQtys has 20 levels");
  check(md.askPrices.size() == kMarketDepthLevels, "askPrices has 20 levels");
  check(md.askQtys.size() == kMarketDepthLevels, "askQtys has 20 levels");
  check(kMarketDepthLevels == 20 && kCtpDepthLevels == 5, "level constants");

  check(md.bidPrices[0] == 3420.0 && md.bidQtys[0] == 5, "bid level 1 copied");
  check(md.bidPrices[4] == 3416.0 && md.bidQtys[4] == 1, "bid level 5 copied");
  check(md.askPrices[0] == 3421.0 && md.askQtys[0] == 6, "ask level 1 copied");
  check(md.askPrices[4] == 3425.0 && md.askQtys[4] == 10, "ask level 5 copied");

  // Levels 6..20 are zero, never a repeat of level 5.
  check(md.bidPrices[5] == 0.0, "bid level 6 price is zero");
  check(md.bidQtys[5] == 0, "bid level 6 qty is zero");
  check(md.askPrices[19] == 0.0, "ask level 20 price is zero");
  check(md.askQtys[19] == 0, "ask level 20 qty is zero");
  bool tailEmpty = true;
  for (std::size_t level = kCtpDepthLevels; level < kMarketDepthLevels; ++level) {
    tailEmpty = tailEmpty && md.bidPrices[level] == 0.0 && md.bidQtys[level] == 0 &&
                md.askPrices[level] == 0.0 && md.askQtys[level] == 0;
  }
  check(tailEmpty, "levels 6..20 are all zero on both sides");

  // Header fields survive the mapping.
  check(md.instrumentId == "rb2601", "instrumentId copied");
  check(md.exchange == "SHFE", "exchange copied");
  check(md.lastPrice == 3421.0, "lastPrice copied");
  check(md.volume == 12345, "volume copied");
  check(md.turnover == 4.2e8, "turnover copied");
  check(md.openInterest == 98765.0, "openInterest copied");
  check(md.updateTime == "10:15:00" && md.updateMillisec == 500, "timestamp copied");

  // CTP marks missing levels with DBL_MAX; the contract means absent by zero.
  DepthTick sparse = fakeTick();
  sparse.bidPrices[3] = std::numeric_limits<double>::max();
  sparse.bidQtys[3] = 7;
  sparse.askPrices[4] = std::numeric_limits<double>::quiet_NaN();
  const MarketData sparseMd = mapDepthMarketData(sparse);
  check(sparseMd.bidPrices[3] == 0.0, "DBL_MAX price maps to zero");
  check(sparseMd.bidQtys[3] == 7, "price sanitising leaves the qty alone");
  check(sparseMd.askPrices[4] == 0.0, "NaN price maps to zero");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_md: 5 CTP levels mapped to 20, 6..20 zero\n";
  return 0;
}

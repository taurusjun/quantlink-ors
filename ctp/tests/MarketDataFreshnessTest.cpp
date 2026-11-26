#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

#include "ors/ctp/MarketDataFreshness.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using namespace std::chrono_literals;
  using ors::ctp::ctpTimestampToNanos;
  using ors::ctp::isSameMarketData;
  using ors::ctp::isMarketDataStale;

  const auto now = std::chrono::steady_clock::now();
  check(isMarketDataStale(now - 2s, now, 1s), "older than limit is stale");
  check(!isMarketDataStale(now - 1s, now, 1s), "equal to limit is not stale");
  check(!isMarketDataStale(now, now, 1s), "fresh tick is not stale");

  const std::uint64_t nanos = ctpTimestampToNanos("20261126", "11:02:49", 500);
  check(nanos != 0, "CTP trading day/time converts to epoch nanos");
  check(nanos % 1000000000ULL == 500000000ULL, "milliseconds are preserved");
  const std::time_t seconds = static_cast<std::time_t>(nanos / 1000000000ULL);
  std::tm local{};
  localtime_r(&seconds, &local);
  check(local.tm_year + 1900 == 2026 && local.tm_mon + 1 == 11 &&
            local.tm_mday == 26 && local.tm_hour == 11 && local.tm_min == 2 &&
            local.tm_sec == 49,
        "trading day and update time map to the right local fields");
  check(ctpTimestampToNanos("bad", "11:02:49", 0) == 0,
        "invalid trading day yields zero");
  check(ctpTimestampToNanos("20261126", "bad", 0) == 0,
        "invalid update time yields zero");

  ors::ctp::MarketData left;
  left.instrumentId = "au2612";
  left.updateTime = "11:02:49";
  left.updateMillisec = 500;
  left.lastPrice = 735.0;
  left.volume = 10;
  left.bidPrices[0] = 734.9;
  ors::ctp::MarketData right = left;
  right.ingestTimestamp = 123;
  check(isSameMarketData(left, right), "ingest timestamp is ignored");
  right.lastPrice = 736.0;
  check(!isSameMarketData(left, right), "price change is a new tick");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_market_data_freshness: stale, timestamp and duplicate rules\n";
  return 0;
}

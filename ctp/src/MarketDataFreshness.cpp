#include "ors/ctp/MarketDataFreshness.h"

#include <ctime>

namespace ors::ctp {
namespace {

bool digits(const std::string& text, std::size_t offset, std::size_t count,
            int& value) {
  if (offset + count > text.size()) {
    return false;
  }
  value = 0;
  for (std::size_t index = 0; index < count; ++index) {
    const char character = text[offset + index];
    if (character < '0' || character > '9') {
      return false;
    }
    value = value * 10 + (character - '0');
  }
  return true;
}

bool twoDigits(const std::string& text, std::size_t offset, int& value) {
  return digits(text, offset, 2, value);
}

}  // namespace

bool isMarketDataStale(std::chrono::steady_clock::time_point last,
                       std::chrono::steady_clock::time_point now,
                       std::chrono::milliseconds limit) {
  return now - last > limit;
}

std::uint64_t ctpTimestampToNanos(const std::string& tradingDay,
                                  const std::string& updateTime,
                                  int millisec) {
  if (tradingDay.size() != 8 || updateTime.size() != 8 ||
      updateTime[2] != ':' || updateTime[5] != ':' || millisec < 0 ||
      millisec > 999) {
    return 0;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (!digits(tradingDay, 0, 4, year) || !twoDigits(tradingDay, 4, month) ||
      !twoDigits(tradingDay, 6, day) || !twoDigits(updateTime, 0, hour) ||
      !twoDigits(updateTime, 3, minute) || !twoDigits(updateTime, 6, second)) {
    return 0;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
      minute > 59 || second > 59) {
    return 0;
  }

  std::tm local{};
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_mday = day;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = second;
  local.tm_isdst = -1;
  const std::time_t seconds = std::mktime(&local);
  if (seconds < 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(seconds) * 1000000000ULL +
         static_cast<std::uint64_t>(millisec) * 1000000ULL;
}

bool isSameMarketData(const MarketData& left, const MarketData& right) {
  return left.instrumentId == right.instrumentId &&
         left.exchange == right.exchange &&
         left.lastPrice == right.lastPrice &&
         left.volume == right.volume &&
         left.turnover == right.turnover &&
         left.openInterest == right.openInterest &&
         left.updateTime == right.updateTime &&
         left.updateMillisec == right.updateMillisec &&
         left.exchangeTimestamp == right.exchangeTimestamp &&
         left.bidPrices == right.bidPrices && left.bidQtys == right.bidQtys &&
         left.askPrices == right.askPrices && left.askQtys == right.askQtys;
}

}  // namespace ors::ctp

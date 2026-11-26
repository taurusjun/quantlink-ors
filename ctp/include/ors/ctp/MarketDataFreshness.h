#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "ors/ctp/CtpSession.h"

namespace ors::ctp {

bool isMarketDataStale(std::chrono::steady_clock::time_point last,
                       std::chrono::steady_clock::time_point now,
                       std::chrono::milliseconds limit);

/// Converts a CTP trading day and HH:MM:SS time to epoch nanoseconds.
/// Returns 0 when the input is malformed.
std::uint64_t ctpTimestampToNanos(const std::string& tradingDay,
                                  const std::string& updateTime,
                                  int millisec);

/// True when two ticks carry the same market state. Ingest time is ignored.
bool isSameMarketData(const MarketData& left, const MarketData& right);

}  // namespace ors::ctp

#pragma once

#include <atomic>
#include <cstdint>

namespace ors::base {

/// Hot-path counters only; increments use relaxed atomics.
struct Metrics {
  std::atomic<std::uint64_t> ordersReceived{0};
  std::atomic<std::uint64_t> ordersSent{0};
  std::atomic<std::uint64_t> ctpOrderCallbacks{0};
  std::atomic<std::uint64_t> ctpTradeCallbacks{0};
  std::atomic<std::uint64_t> aeronPublishOk{0};
  std::atomic<std::uint64_t> aeronBackPressure{0};
  std::atomic<std::uint64_t> aeronFailed{0};
  std::atomic<std::uint64_t> unknownReports{0};
  std::atomic<std::uint64_t> staleFrontReports{0};
  std::atomic<std::uint64_t> mdTicks{0};
  std::atomic<std::uint64_t> mdStale{0};
  std::atomic<std::uint64_t> reconnects{0};
  std::atomic<std::uint64_t> queryFailures{0};
};

}  // namespace ors::base

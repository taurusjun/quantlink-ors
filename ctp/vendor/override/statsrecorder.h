#pragma once

#include <cstdint>

class StatsRecorder {
 public:
  template <typename... Args>
  static int registerNewStat(Args&&...) {
    return 0;
  }
  static void record(int, std::int64_t) {}
};

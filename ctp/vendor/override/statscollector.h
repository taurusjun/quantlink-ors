#pragma once

#include <cstdint>
#include <string>

namespace illuminati {
namespace stats {

template <typename T = void>
class StatsCollector {
 public:
  static StatsCollector& getInstance() {
    static StatsCollector instance;
    return instance;
  }
  void init(const std::string&) {}
  template <typename... Args>
  static int registerNewStat(Args&&...) {
    return 0;
  }
  static void record(int, std::int64_t) {}
};

}  // namespace stats
}  // namespace illuminati

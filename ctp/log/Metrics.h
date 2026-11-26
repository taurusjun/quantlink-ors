#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ors {

/// Low-overhead named counters/latencies. No per-event log output.
class Metrics {
 public:
  void init(const std::string&) {}
  int registerStat(const std::string& name, std::int64_t, std::int64_t) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = ids_.find(name);
    if (found != ids_.end()) return found->second;
    const int id = static_cast<int>(counters_.size());
    ids_.emplace(name, id);
    counters_.emplace_back(std::make_unique<Counter>());
    return id;
  }
  void record(int id, std::int64_t value) {
    if (id < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    while (id >= static_cast<int>(counters_.size())) {
      counters_.emplace_back(std::make_unique<Counter>());
    }
    counters_[id]->count.fetch_add(1, std::memory_order_relaxed);
    counters_[id]->total_ns.fetch_add(value, std::memory_order_relaxed);
  }

 private:
  struct Counter {
    std::atomic<std::uint64_t> count{0};
    std::atomic<std::int64_t> total_ns{0};
  };
  std::mutex mutex_;
  std::unordered_map<std::string, int> ids_;
  std::vector<std::unique_ptr<Counter>> counters_;
};

inline Metrics& metrics() {
  static Metrics instance;
  return instance;
}

}  // namespace ors

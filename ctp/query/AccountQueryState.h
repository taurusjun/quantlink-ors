#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace ors {

class AccountQueryState {
 public:
  static constexpr std::chrono::milliseconds kCadence{1000};

  bool tryBegin(std::uint64_t requestId,
                std::chrono::steady_clock::time_point now,
                std::chrono::milliseconds timeout);
  void bindCtpRequest(int ctpRequestId);
  std::optional<std::uint64_t> requestIdFor(int ctpRequestId) const;
  std::optional<std::uint64_t> expire(
      std::chrono::steady_clock::time_point now);
  void finish();
  bool pending() const { return pending_; }

 private:
  bool pending_{false};
  std::uint64_t requestId_{0};
  int ctpRequestId_{0};
  std::chrono::steady_clock::time_point lastSent_{};
  std::chrono::steady_clock::time_point deadline_{};
};

}  // namespace ors

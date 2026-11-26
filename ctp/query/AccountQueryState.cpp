#include "query/AccountQueryState.h"

namespace ors {

bool AccountQueryState::tryBegin(
    std::uint64_t requestId, std::chrono::steady_clock::time_point now,
    std::chrono::milliseconds timeout) {
  if (requestId == 0 || pending_) {
    return false;
  }
  if (lastSent_ != std::chrono::steady_clock::time_point{} &&
      now - lastSent_ < kCadence) {
    return false;
  }

  pending_ = true;
  requestId_ = requestId;
  ctpRequestId_ = 0;
  lastSent_ = now;
  deadline_ = now + timeout;
  return true;
}

void AccountQueryState::bindCtpRequest(int ctpRequestId) {
  if (pending_) {
    ctpRequestId_ = ctpRequestId;
  }
}

std::optional<std::uint64_t> AccountQueryState::requestIdFor(
    int ctpRequestId) const {
  if (!pending_ || ctpRequestId_ == 0 || ctpRequestId_ != ctpRequestId) {
    return std::nullopt;
  }
  return requestId_;
}

std::optional<std::uint64_t> AccountQueryState::expire(
    std::chrono::steady_clock::time_point now) {
  if (!pending_ || now < deadline_) {
    return std::nullopt;
  }
  const std::uint64_t requestId = requestId_;
  finish();
  return requestId;
}

void AccountQueryState::finish() {
  pending_ = false;
  requestId_ = 0;
  ctpRequestId_ = 0;
  deadline_ = {};
}

}  // namespace ors

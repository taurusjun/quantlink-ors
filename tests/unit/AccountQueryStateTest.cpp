#include <chrono>
#include <iostream>

#include "query/AccountQueryState.h"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;
using std::chrono::seconds;

void rejectsSecondQueryInsideInterval() {
  ::ors::AccountQueryState state;
  const auto now = Clock::now();
  check(state.tryBegin(1, now, seconds(5)), "first query accepted");
  check(!state.tryBegin(2, now + milliseconds(100), seconds(5)),
        "second query rejected inside cadence");
}

void mapsCtpRequestIdBackToSdkRequestId() {
  ::ors::AccountQueryState state;
  check(state.tryBegin(9, Clock::now(), seconds(5)), "query accepted");
  state.bindCtpRequest(77);
  check(state.requestIdFor(77) == 9, "request id maps back");
  check(!state.requestIdFor(78).has_value(), "wrong ctp id rejected");
}

void expireReturnsRequestIdAndClearsPending() {
  ::ors::AccountQueryState state;
  const auto now = Clock::now();
  check(state.tryBegin(11, now, milliseconds(100)), "query accepted");
  check(!state.expire(now + milliseconds(50)).has_value(),
        "not expired before deadline");
  const auto expired = state.expire(now + milliseconds(100));
  check(expired == 11, "expired request id returned");
  check(!state.pending(), "pending cleared after expiry");
}

void lateResponseAfterExpireIsIgnored() {
  ::ors::AccountQueryState state;
  const auto now = Clock::now();
  state.tryBegin(12, now, milliseconds(100));
  state.bindCtpRequest(88);
  state.expire(now + milliseconds(100));
  check(!state.requestIdFor(88).has_value(), "late response ignored");
}

void finishAllowsNextQuery() {
  ::ors::AccountQueryState state;
  const auto now = Clock::now();
  state.tryBegin(13, now, seconds(5));
  state.finish();
  check(state.tryBegin(14, now + seconds(2), seconds(5)),
        "next query after finish and cadence");
}

}  // namespace

int main() {
  rejectsSecondQueryInsideInterval();
  mapsCtpRequestIdBackToSdkRequestId();
  expireReturnsRequestIdAndClearsPending();
  lateResponseAfterExpireIsIgnored();
  finishAllowsNextQuery();
  return failures == 0 ? 0 : 1;
}

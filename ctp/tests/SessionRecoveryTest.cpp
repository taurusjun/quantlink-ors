#include <iostream>
#include <string>

#include "ors/ctp/SessionRecovery.h"

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
  using ors::ctp::SessionState;
  using ors::ctp::canLoginAfterDisconnect;
  using ors::ctp::isTerminalSessionFailure;

  check(canLoginAfterDisconnect(SessionState::Disconnected),
        "disconnected sessions may log in again");
  check(!canLoginAfterDisconnect(SessionState::Failed),
        "failed sessions are not retryable");
  check(!canLoginAfterDisconnect(SessionState::Ready),
        "ready sessions do not log in again");

  check(isTerminalSessionFailure(SessionState::Failed),
        "failed is terminal");
  check(!isTerminalSessionFailure(SessionState::Disconnected),
        "disconnected is recoverable");
  check(!isTerminalSessionFailure(SessionState::Ready),
        "ready is not terminal");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_session_recovery: reconnect only from disconnected\n";
  return 0;
}

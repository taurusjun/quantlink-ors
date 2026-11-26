#include "ors/ctp/SessionRecovery.h"

namespace ors::ctp {

bool canLoginAfterDisconnect(SessionState state) {
  return state == SessionState::Disconnected;
}

bool isTerminalSessionFailure(SessionState state) {
  return state == SessionState::Failed;
}

}  // namespace ors::ctp

#pragma once

#include "ors/ctp/CtpSession.h"

namespace ors::ctp {

/// A clean front disconnect may log in again; Failed is terminal.
bool canLoginAfterDisconnect(SessionState state);

bool isTerminalSessionFailure(SessionState state);

}  // namespace ors::ctp

#pragma once

#include <cstdint>

#include "domain/PositionTable.h"

namespace ors {

struct OffsetDecision {
  char flag{0};
  int orderType{0};
};

/// Maps a position reservation to CTP CombOffsetFlag without mutating state.
/// This is the pure part of the company ORS SetCombOffsetFlag logic.
OffsetDecision chooseOffset(const OffsetReservation& reservation,
                            std::int16_t exchangeType);

}  // namespace ors

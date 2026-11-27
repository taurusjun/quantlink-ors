#include "execution/OffsetPolicy.h"

#include <ThostFtdcUserApiDataType.h>

namespace ors {

OffsetDecision chooseOffset(const OffsetReservation& reservation,
                            std::int16_t exchangeType) {
  if (!reservation.valid) {
    return {THOST_FTDC_OF_Open, static_cast<int>(PositionAction::Open)};
  }
  switch (reservation.action) {
    case PositionAction::CloseToday:
      return {exchangeType == 49 /* CHINA_SHFE */
                  ? THOST_FTDC_OF_CloseToday
                  : THOST_FTDC_OF_Close,
              static_cast<int>(PositionAction::CloseToday)};
    case PositionAction::CloseYesterday:
      return {THOST_FTDC_OF_Close,
              static_cast<int>(PositionAction::CloseYesterday)};
    case PositionAction::Open:
    default:
      return {THOST_FTDC_OF_Open, static_cast<int>(PositionAction::Open)};
  }
}

}  // namespace ors

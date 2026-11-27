#include <iostream>

#include <ThostFtdcUserApiDataType.h>

#include "domain/PositionTable.h"
#include "execution/OffsetPolicy.h"

int main() {
  ::ors::OffsetReservation open;
  auto d = ::ors::chooseOffset(open, 49);
  if (d.flag != THOST_FTDC_OF_Open || d.orderType != 3) {
    std::cerr << "open mapping wrong\n";
    return 1;
  }

  ::ors::OffsetReservation today;
  today.valid = true;
  today.action = ::ors::PositionAction::CloseToday;
  d = ::ors::chooseOffset(today, 49);
  if (d.flag != THOST_FTDC_OF_CloseToday || d.orderType != 1) {
    std::cerr << "SHFE close-today mapping wrong\n";
    return 1;
  }
  d = ::ors::chooseOffset(today, 50);
  if (d.flag != THOST_FTDC_OF_Close || d.orderType != 1) {
    std::cerr << "non-SHFE close-today mapping wrong\n";
    return 1;
  }

  ::ors::OffsetReservation yesterday;
  yesterday.valid = true;
  yesterday.action = ::ors::PositionAction::CloseYesterday;
  d = ::ors::chooseOffset(yesterday, 49);
  if (d.flag != THOST_FTDC_OF_Close || d.orderType != 2) {
    std::cerr << "close-yesterday mapping wrong\n";
    return 1;
  }
  return 0;
}

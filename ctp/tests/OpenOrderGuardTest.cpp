#include <iostream>
#include <vector>

#include "control/OpenOrderGuard.h"

int main() {
  CThostFtdcOrderField terminal{};
  terminal.OrderStatus = THOST_FTDC_OST_AllTraded;
  CThostFtdcOrderField canceled{};
  canceled.OrderStatus = THOST_FTDC_OST_Canceled;
  CThostFtdcOrderField live{};
  live.OrderStatus = THOST_FTDC_OST_NoTradeQueueing;

  if (::ors::hasLiveOrders({terminal, canceled})) {
    std::cerr << "terminal rows treated as live\n";
    return 1;
  }
  if (!::ors::hasLiveOrders({terminal, live})) {
    std::cerr << "live row missed\n";
    return 1;
  }
  return 0;
}

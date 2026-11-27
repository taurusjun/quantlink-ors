#include "execution/OpenOrderGuard.h"

namespace ors {

bool hasLiveOrders(const std::vector<CThostFtdcOrderField>& orders) {
  for (const auto& order : orders) {
    if (order.OrderStatus != THOST_FTDC_OST_AllTraded &&
        order.OrderStatus != THOST_FTDC_OST_Canceled) {
      return true;
    }
  }
  return false;
}

}  // namespace ors

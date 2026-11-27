#include <iostream>
#include <cstring>

#include "execution/OrderService.h"

int main() {
  ::ors::OrderStore store;
  ::ors::OrderService service(store, [](::ors::infra::ResponseMsg*) {});
  service.setNextOrderRef(41);
  if (service.takeOrderRef() != "000000000041" ||
      service.takeOrderRef() != "000000000042") {
    std::cerr << "order ref allocation wrong\n";
    return 1;
  }

  ::ors::infra::RequestMsg buy{};
  buy.OrderID = 1;
  buy.Transaction_Type = 'B';
  buy.Price = 120;
  std::strcpy(buy.Contract_Description.Symbol, "au2612");
  ::ors::OrderInfo open{};
  open.Transaction_Type = 'S';
  open.Price = 110;
  std::strcpy(open.ScripCode, "au2612");
  store.local()[1] = open;
  if (!service.crossCheck(buy)) {
    std::cerr << "crossing order was not detected\n";
    return 1;
  }
  buy.Price = 90;
  if (service.crossCheck(buy)) {
    std::cerr << "non-crossing order rejected\n";
    return 1;
  }
  return 0;
}

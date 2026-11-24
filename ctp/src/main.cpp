#include <iostream>

#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"

int main() {
  // Link smoke: take each SDK factory's address without starting a session.
  volatile auto traderFactory = &CThostFtdcTraderApi::CreateFtdcTraderApi;
  volatile auto mdFactory = &CThostFtdcMdApi::CreateFtdcMdApi;
  (void)traderFactory;
  (void)mdFactory;

  std::cout << "quantlink-ors ctp skeleton\n";
  return 0;
}

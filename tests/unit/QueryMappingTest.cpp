#include <cmath>
#include <iostream>
#include <limits>

#include "query/QueryMapping.h"

int main() {
  CThostFtdcTradingAccountField row{};
  row.Balance = 1000.0;
  row.Available = std::numeric_limits<double>::max();
  row.CurrMargin = 10.0;
  row.TradingDay[0] = '2';
  row.TradingDay[1] = '0';
  row.TradingDay[2] = '2';
  row.TradingDay[3] = '6';
  row.TradingDay[4] = '1';
  row.TradingDay[5] = '1';
  row.TradingDay[6] = '2';
  row.TradingDay[7] = '6';

  ::ors::OrsConfig config;
  config.accountId = "142266";
  const auto snapshot = ::ors::mapAccountSnapshot(row, 7, config);
  if (snapshot.requestId != 7 || snapshot.accountId != "142266" ||
      snapshot.balance != 1000.0 || snapshot.available != 0.0 ||
      snapshot.margin != 10.0) {
    std::cerr << "account mapping failed\n";
    return 1;
  }
  return 0;
}

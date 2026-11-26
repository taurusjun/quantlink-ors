#include <iostream>
#include <string>

#include "ors/ctp/QueryCadence.h"

int main() {
  using ors::ctp::QueryKind;
  using ors::ctp::nextQuery;

  if (nextQuery(QueryKind::Account) != QueryKind::Position ||
      nextQuery(QueryKind::Position) != QueryKind::Account) {
    std::cerr << "FAIL: account and position must alternate\n";
    return 1;
  }
  std::cout << "ctp_query_cadence: account/position alternate\n";
  return 0;
}

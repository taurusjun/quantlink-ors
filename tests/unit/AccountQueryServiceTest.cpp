#include <cstring>
#include <iostream>

#include "query/AccountQueryService.h"
#include "transport/InMemoryOrsTransport.h"

int main() {
  ::ors::OrsConfig config;
  config.brokerId = "9999";
  config.investorId = "142266";
  config.accountId = "142266";
  config.accountQueryTimeoutMs = 5000;

  ::ors::InMemoryOrsTransport transport;
  int requested = 0;
  ::ors::AccountQueryService service(config, transport,
      [&](CThostFtdcQryTradingAccountField& query) {
        ++requested;
        if (std::strcmp(query.BrokerID, "9999") != 0 ||
            std::strcmp(query.InvestorID, "142266") != 0) {
          return -1;
        }
        return 42;
      });

  service.query(::ors::AccountQueryRequest::of(7, "142266"));
  if (requested != 1 || transport.accounts().size() != 0) {
    std::cerr << "query was not sent exactly once\n";
    return 1;
  }

  CThostFtdcTradingAccountField account{};
  account.Available = 1234.5;
  account.Balance = 2000.0;
  service.onResponse(&account, nullptr, 42, true);
  if (transport.accounts().size() != 1 ||
      transport.accounts().front().requestId != 7 ||
      transport.accounts().front().errorCode != 0 ||
      transport.accounts().front().available != 1234.5) {
    std::cerr << "account response mapping failed\n";
    return 1;
  }

  service.query(::ors::AccountQueryRequest::of(8, "142266"));
  if (transport.accounts().size() != 2 ||
      transport.accounts().back().errorCode != 3) {
    std::cerr << "throttle error not published\n";
    return 1;
  }
  return 0;
}

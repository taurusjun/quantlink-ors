#include <iostream>
#include <string>

#include "ors/ctp/InboundValidation.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using ors::ctp::OrderRequest;
  using ors::ctp::CancelRequest;
  using ors::ctp::validateInboundCancel;
  using ors::ctp::validateInboundOrder;

  OrderRequest order;
  order.accountId = "142266";
  order.venueId = "SIMNOW";
  order.environment = "SHADOW";
  auto reason = validateInboundOrder(order, "SIMNOW", "142266", "SHADOW");
  check(!reason.has_value(), "matching order identity passes");

  OrderRequest wrongAccount = order;
  wrongAccount.accountId = "900001";
  reason = validateInboundOrder(wrongAccount, "SIMNOW", "142266", "SHADOW");
  check(reason.has_value() && reason->find("account") != std::string::npos,
        "account mismatch is named");

  OrderRequest wrongVenue = order;
  wrongVenue.venueId = "OTHER";
  reason = validateInboundOrder(wrongVenue, "SIMNOW", "142266", "SHADOW");
  check(reason.has_value() && reason->find("venue") != std::string::npos,
        "venue mismatch is named");

  OrderRequest wrongEnvironment = order;
  wrongEnvironment.environment = "PROD";
  reason = validateInboundOrder(wrongEnvironment, "SIMNOW", "142266", "SHADOW");
  check(reason.has_value() && reason->find("environment") != std::string::npos,
        "environment mismatch is named");

  CancelRequest cancel;
  cancel.accountId = "142266";
  cancel.venueId = "SIMNOW";
  cancel.environment = "SHADOW";
  check(!validateInboundCancel(cancel, "SIMNOW", "142266", "SHADOW").has_value(),
        "matching cancel identity passes");
  cancel.environment = "PROD";
  reason = validateInboundCancel(cancel, "SIMNOW", "142266", "SHADOW");
  check(reason.has_value(), "cancel environment mismatch is rejected");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_inbound_validation: account, venue and environment\n";
  return 0;
}

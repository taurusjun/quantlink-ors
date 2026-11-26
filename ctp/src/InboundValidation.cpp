#include "ors/ctp/InboundValidation.h"

namespace ors::ctp {
namespace {

std::optional<std::string> validate(std::string_view accountId,
                                    std::string_view venueId,
                                    std::string_view environment,
                                    std::string_view expectedVenue,
                                    std::string_view expectedAccount,
                                    std::string_view expectedEnvironment) {
  if (accountId != expectedAccount) {
    return "account mismatch";
  }
  if (venueId != expectedVenue) {
    return "venue mismatch";
  }
  if (environment != expectedEnvironment) {
    return "environment mismatch";
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::string> validateInboundOrder(
    const OrderRequest& request, std::string_view venueId,
    std::string_view accountId, std::string_view environment) {
  return validate(request.accountId, request.venueId, request.environment,
                  venueId, accountId, environment);
}

std::optional<std::string> validateInboundCancel(
    const CancelRequest& request, std::string_view venueId,
    std::string_view accountId, std::string_view environment) {
  return validate(request.accountId, request.venueId, request.environment,
                  venueId, accountId, environment);
}

}  // namespace ors::ctp

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ors/ctp/CtpAdapter.h"

namespace ors::ctp {

/// Returns the mismatch reason, or nullopt when the request belongs to this
/// venue account and environment.
std::optional<std::string> validateInboundOrder(
    const OrderRequest& request, std::string_view venueId,
    std::string_view accountId, std::string_view environment);

std::optional<std::string> validateInboundCancel(
    const CancelRequest& request, std::string_view venueId,
    std::string_view accountId, std::string_view environment);

}  // namespace ors::ctp

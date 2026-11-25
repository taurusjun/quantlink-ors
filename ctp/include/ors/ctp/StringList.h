#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ors::ctp {

/// Splits a comma/space separated list, dropping empty entries.
std::vector<std::string> splitList(std::string_view value);

}  // namespace ors::ctp

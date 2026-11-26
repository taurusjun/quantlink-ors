#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ors::base {

std::vector<std::string> splitList(std::string_view value);

}  // namespace ors::base

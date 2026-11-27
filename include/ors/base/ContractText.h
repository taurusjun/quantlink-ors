#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ors::base {

/// Truncates to at most maxBytes without splitting a UTF-8 character.
std::string fitContractField(std::string_view text, std::size_t maxBytes);

/// Rejects a value that cannot fit a fixed-width contract field. The error
/// names the key and byte counts, never the value.
void requireContractWidth(std::string_view value, std::size_t maxBytes,
                          const std::string& key);

}  // namespace ors::base

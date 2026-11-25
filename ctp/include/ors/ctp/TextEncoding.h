#pragma once

#include <string>
#include <string_view>

namespace ors::ctp {

/// Trims leading/trailing ASCII spaces. CTP pads fixed-width ID fields with
/// spaces; the contract IDs must be clean.
std::string trimAsciiSpace(std::string_view input);

/// Converts a CTP GBK/GB18030 error message to UTF-8. Invalid input is
/// returned unchanged so a bad venue message never breaks execution flow.
std::string gbkToUtf8(std::string_view input);

}  // namespace ors::ctp

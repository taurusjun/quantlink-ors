#include "ors/base/ContractText.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ors::base {
namespace {

std::size_t utf8SequenceLength(unsigned char lead) {
  if ((lead & 0x80U) == 0) {
    return 1;
  }
  if ((lead & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((lead & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((lead & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 0;
}

}  // namespace

std::string fitContractField(std::string_view text, std::size_t maxBytes) {
  std::size_t end = std::min(text.size(), maxBytes);
  while (end > 0) {
    std::size_t start = end - 1;
    while (start > 0 &&
           (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) {
      --start;
    }
    const std::size_t length =
        utf8SequenceLength(static_cast<unsigned char>(text[start]));
    if (length != 0 && start + length <= end) {
      break;
    }
    end = start;
  }
  return std::string(text.substr(0, end));
}

void requireContractWidth(std::string_view value, std::size_t maxBytes,
                          const std::string& key) {
  if (value.size() <= maxBytes) {
    return;
  }
  throw std::runtime_error(key + " is " + std::to_string(value.size()) +
                           " bytes, contract limit is " +
                           std::to_string(maxBytes));
}

}  // namespace ors::base

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ors/base/ContractText.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

template <typename Fn>
bool throwsContaining(Fn fn, const std::string& needle) {
  try {
    fn();
  } catch (const std::runtime_error& error) {
    return std::string(error.what()).find(needle) != std::string::npos;
  }
  return false;
}

}  // namespace

int main() {
  using ors::base::fitContractField;
  using ors::base::requireContractWidth;

  check(fitContractField("abc", 8) == "abc", "short ASCII is unchanged");
  check(fitContractField("你好世界", 7) == "你好",
        "UTF-8 truncation backs off the incomplete character");
  check(fitContractField("你好世界", 6) == "你好", "exact UTF-8 boundary");
  check(fitContractField("你好世界", 9) == "你好世", "next complete UTF-8 prefix");
  check(fitContractField(std::string(100, 'a'), 64) == std::string(64, 'a'),
        "long ASCII fits to the exact field width");
  check(fitContractField("abc", 0).empty(), "zero width yields empty");

  requireContractWidth("SIMNOW", 32, "VENUE_ID");
  requireContractWidth(std::string(16, 'x'), 16, "ENVIRONMENT");
  check(throwsContaining(
            [] { requireContractWidth(std::string(33, 'x'), 32, "VENUE_ID"); },
            "VENUE_ID"),
        "over-width value names the key");
  check(throwsContaining(
            [] { requireContractWidth("你好世界", 8, "ORS_ID"); }, "ORS_ID"),
        "width is measured in UTF-8 bytes");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "base_contract_text: UTF-8 safe fixed-width fields\n";
  return 0;
}

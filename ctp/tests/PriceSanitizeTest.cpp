#include <cmath>
#include <iostream>
#include <limits>

#include "ors/ctp/PriceSanitize.h"

namespace {
int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}
}  // namespace

int main() {
  using ors::ctp::sanitizeMoney;
  using ors::ctp::sanitizePrice;
  using ors::ctp::sanitizeQty;

  check(sanitizePrice(0.0) == 0.0, "zero price stays zero");
  check(sanitizePrice(777.5) == 777.5, "normal price passes");
  check(sanitizePrice(-1.0) == 0.0, "negative price is absent");
  check(sanitizePrice(std::numeric_limits<double>::max()) == 0.0,
        "DBL_MAX price is absent");
  check(sanitizePrice(std::nan("")) == 0.0, "NaN price is absent");
  check(sanitizePrice(1.0e12) == 0.0, "1e12 price is absent");

  check(sanitizeMoney(-1.0) == -1.0, "negative balance is preserved");
  check(sanitizeMoney(std::numeric_limits<double>::max()) == 0.0,
        "DBL_MAX balance is absent");

  check(sanitizeQty(1) == 1, "positive qty passes");
  check(sanitizeQty(0) == 0, "zero qty stays zero");
  check(sanitizeQty(-1) == 0, "negative qty is zero");

  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "price sanitize tests passed\n";
  return 0;
}

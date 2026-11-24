#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ors::ctp {

/// CTP uses DBL_MAX for an empty price. Prices at or above 1e12 are not real
/// futures/option prices and must reach the contract as "absent" (0).
inline double sanitizePrice(double value) {
  if (!std::isfinite(value) || std::fabs(value) >= 1.0e12) return 0.0;
  return value > 0.0 ? value : 0.0;
}

/// Account amounts may legitimately be negative; only non-finite and the
/// CTP missing-value sentinel are replaced.
inline double sanitizeMoney(double value) {
  if (!std::isfinite(value)
      || std::fabs(value) >= std::numeric_limits<double>::max()) {
    return 0.0;
  }
  return value;
}

inline int sanitizeQty(int qty) {
  return qty > 0 ? qty : 0;
}

}  // namespace ors::ctp

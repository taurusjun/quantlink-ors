#include "ors/ctp/OfferRetry.h"

#include <thread>

namespace ors::ctp {

OfferResult offerWithRetry(const std::function<OfferResult()>& offer,
                           std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (true) {
    const OfferResult result = offer();
    if (result != OfferResult::BackPressured) {
      return result;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

}  // namespace ors::ctp

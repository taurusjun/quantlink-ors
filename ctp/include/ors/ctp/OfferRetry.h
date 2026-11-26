#pragma once

#include <chrono>
#include <functional>

namespace ors::ctp {

enum class OfferResult {
  Published,
  BackPressured,
  NotConnected,
  Closed,
};

/// Retries only BackPressured until the deadline; other results are final.
OfferResult offerWithRetry(const std::function<OfferResult()>& offer,
                           std::chrono::milliseconds timeout);

}  // namespace ors::ctp

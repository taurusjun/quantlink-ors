#include <chrono>
#include <iostream>
#include <string>

#include "ors/ctp/OfferRetry.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using ors::ctp::OfferResult;
  using ors::ctp::offerWithRetry;

  int calls = 0;
  OfferResult result = offerWithRetry(
      [&] {
        ++calls;
        return calls < 3 ? OfferResult::BackPressured : OfferResult::Published;
      },
      std::chrono::milliseconds(100));
  check(result == OfferResult::Published, "back-pressure retries to published");
  check(calls == 3, "stops after the first successful offer");

  calls = 0;
  result = offerWithRetry(
      [&] {
        ++calls;
        return OfferResult::NotConnected;
      },
      std::chrono::milliseconds(100));
  check(result == OfferResult::NotConnected, "not-connected is not retried");
  check(calls == 1, "not-connected returns immediately");

  calls = 0;
  result = offerWithRetry(
      [&] {
        ++calls;
        return OfferResult::Closed;
      },
      std::chrono::milliseconds(100));
  check(result == OfferResult::Closed, "closed is not retried");
  check(calls == 1, "closed returns immediately");

  calls = 0;
  result = offerWithRetry(
      [&] {
        ++calls;
        return OfferResult::BackPressured;
      },
      std::chrono::milliseconds(5));
  check(result == OfferResult::BackPressured, "timeout returns the last failure");
  check(calls > 1, "timeout kept retrying");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_offer_retry: publish, back-pressure, timeout mapping\n";
  return 0;
}

#include <iostream>
#include <set>
#include <string>

#include "execution/ClientRegistry.h"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

void allocatesSequentialIds() {
  ::ors::ClientRegistry registry;
  const auto first = registry.registerClient("a");
  const auto second = registry.registerClient("b");
  check(first.ok && first.clientId == 1, "first client id");
  check(second.ok && second.clientId == 2, "second client id");
}

void neverReleasesIdWhileOrderLive() {
  ::ors::ClientRegistry registry;
  const auto client = registry.registerClient("a");
  registry.releaseIfFlat(client.clientId, {::ors::ClientRegistry::makeOrderId(1, 7)});
  const auto next = registry.registerClient("b");
  check(next.clientId == 2, "live order keeps client id reserved");
}

void releasesIdOnlyWhenNoOrder() {
  ::ors::ClientRegistry registry;
  const auto client = registry.registerClient("a");
  registry.releaseIfFlat(client.clientId,
                         {::ors::ClientRegistry::makeOrderId(2, 7)});
  const auto next = registry.registerClient("b");
  check(next.clientId == 2, "unrelated client id is released");
}

void rejectsWhenRangeExhausted() {
  ::ors::ClientRegistry registry;
  ::ors::ClientRegistration result;
  for (std::uint32_t index = 0; index < ::ors::ClientRegistry::kMaxClientId;
       ++index) {
    result = registry.registerClient("x");
  }
  check(result.ok && result.clientId == ::ors::ClientRegistry::kMaxClientId,
        "last client id");
  const auto exhausted = registry.registerClient("x");
  check(!exhausted.ok && exhausted.errorCode == 1, "range exhausted");
}

void validatesOrderOwnership() {
  ::ors::ClientRegistry registry;
  registry.registerClient("tb-a");
  registry.registerClient("tb-b");
  check(registry.ownsOrder(1'000'007), "client 1 owns its range");
  check(registry.ownsOrder(2'000'001), "client 2 owns its range");
  check(!registry.ownsOrder(3'000'001), "unregistered client rejected");
  check(!registry.ownsOrder(0), "zero order id rejected");
}

void orderIdFormula() {
  check(::ors::ClientRegistry::makeOrderId(2, 3) == 2'000'003,
        "company order id formula");
  check(::ors::ClientRegistry::makeOrderId(0, 3) == 0, "client zero rejected");
  check(::ors::ClientRegistry::makeOrderId(2, 1'000'000) == 0,
        "order count overflow rejected");
  check(::ors::ClientRegistry::clientIdFromOrderId(2'000'003) == 2,
        "client id extraction");
}

}  // namespace

int main() {
  allocatesSequentialIds();
  neverReleasesIdWhileOrderLive();
  releasesIdOnlyWhenNoOrder();
  rejectsWhenRangeExhausted();
  validatesOrderOwnership();
  orderIdFormula();
  return failures == 0 ? 0 : 1;
}

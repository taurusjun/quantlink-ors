#include "control/ClientRegistry.h"

namespace ors {

ClientRegistration ClientRegistry::registerClient(std::string_view) {
  if (nextClientId_ > kMaxClientId) {
    return {false, 0, 1, "client id range exhausted"};
  }
  const std::uint32_t clientId = nextClientId_++;
  allocated_.insert(clientId);
  return {true, clientId, 0, {}};
}

void ClientRegistry::releaseIfFlat(
    std::uint32_t clientId, const std::set<std::uint32_t>& orderIds) {
  for (const std::uint32_t orderId : orderIds) {
    if (clientIdFromOrderId(orderId) == clientId) {
      return;
    }
  }
  allocated_.erase(clientId);
}

std::uint32_t ClientRegistry::makeOrderId(std::uint32_t clientId,
                                          std::uint32_t orderCount) {
  if (clientId == 0 || clientId > kMaxClientId ||
      orderCount >= kOrderIdRange) {
    return 0;
  }
  return clientId * kOrderIdRange + orderCount;
}

std::uint32_t ClientRegistry::clientIdFromOrderId(std::uint32_t orderId) {
  return orderId / kOrderIdRange;
}

bool ClientRegistry::hasClientId(std::uint32_t clientId) const {
  return allocated_.count(clientId) != 0;
}

}  // namespace ors

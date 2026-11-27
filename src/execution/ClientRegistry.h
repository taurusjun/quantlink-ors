#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace ors {

struct ClientRegistration {
  bool ok{false};
  std::uint32_t clientId{0};
  std::uint32_t errorCode{0};
  std::string errorMessage;
};

class ClientRegistry {
 public:
  static constexpr std::uint32_t kOrderIdRange = 1'000'000;
  static constexpr std::uint32_t kMaxClientId = 4294;

  ClientRegistration registerClient(std::string_view instanceId);
  void releaseIfFlat(std::uint32_t clientId,
                     const std::set<std::uint32_t>& orderIds);

  static std::uint32_t makeOrderId(std::uint32_t clientId,
                                   std::uint32_t orderCount);
  static std::uint32_t clientIdFromOrderId(std::uint32_t orderId);
  bool hasClientId(std::uint32_t clientId) const;

 private:
  std::uint32_t nextClientId_{1};
  std::set<std::uint32_t> allocated_;
};

}  // namespace ors

#pragma once

#include <cstdint>
#include <string>

namespace ors {

struct AccountQueryRequest {
  std::uint64_t requestId{0};
  std::string accountId;
  std::uint64_t timestamp{0};
};

struct AccountSnapshot {
  std::uint64_t requestId{0};
  std::string accountId;
  std::string tradingDay;
  double balance{0.0};
  double available{0.0};
  double margin{0.0};
  double frozenMargin{0.0};
  double commission{0.0};
  double closeProfit{0.0};
  double positionProfit{0.0};
  double deposit{0.0};
  double withdraw{0.0};
  std::uint32_t errorCode{0};
  std::string errorMsg;
  std::uint64_t timestamp{0};
};

struct SessionEvent {
  std::string venueId;
  std::string accountId;
  std::string orsId;
  std::string state;
  std::string reason;
  std::uint64_t timestamp{0};
};

struct ClientRegisterRequest {
  std::uint64_t requestId{0};
  std::string instanceId;
  std::string accountId;
  std::string environment;
  std::uint64_t timestamp{0};
};

struct ClientRegisterResult {
  std::uint64_t requestId{0};
  std::uint32_t clientId{0};
  std::uint32_t errorCode{0};
  std::string errorMsg;
  std::uint64_t timestamp{0};
};

}  // namespace ors

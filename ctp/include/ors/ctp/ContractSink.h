#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "com_quantlink_contract_sbe/GatewayState.h"
#include "com_quantlink_contract_sbe/SettlementState.h"
#include "com_quantlink_contract_sbe/VenueSessionState.h"
#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/CtpSession.h"

namespace csbe = com::quantlink::contract::sbe;

namespace ors::ctp {

/// Stream ids are the contract defaults; deployment may override them.
struct ContractStreams {
  int execution{2003};
  int position{2004};
  int account{2005};
  int session{2006};
  int marketData{1001};
};

/// Identity and epoch context shared by every contract message in one session.
struct ContractContext {
  std::string venueId;
  std::string accountId;
  std::string orsId;
  std::string environment;
  std::uint64_t sourceSessionId{0};
  std::uint64_t sourcePublicationEpoch{0};
  ContractStreams streams;
};

/// The session payload that is not already carried by the CTP session object.
struct VenueSessionEvent {
  std::string tradingDay;
  csbe::GatewayState::Value loginState{csbe::GatewayState::DOWN};
  csbe::SettlementState::Value settlementState{csbe::SettlementState::NONE};
  std::string adapterSessionId;
  csbe::VenueSessionState::Value state{csbe::VenueSessionState::STARTING};
  std::string reason;
  std::uint64_t snapshotTimestamp{0};
  std::map<std::uint32_t, std::uint64_t> lastSeqByStream;
};

/// Transport seam: the production implementation is the Aeron publisher.
class FramePublisher {
 public:
  virtual ~FramePublisher() = default;
  virtual bool publish(int streamId, const char* data, std::size_t length) = 0;
};

/// Encodes contract 0.2.0 SBE messages and publishes them on L0 streams.
class ContractSink {
 public:
  ContractSink(FramePublisher& publisher, ContractContext context);

  bool publishOrderUpdate(const OrderUpdate& update);
  bool publishTrade(const Trade& trade);
  bool publishMarketData(const MarketData& data);
  bool publishAccountSnapshot(const AccountSnapshot& snapshot);
  bool publishPositionSnapshot(const PositionSnapshot& snapshot);
  bool publishSession(const VenueSessionEvent& session);

 private:
  bool publishFrame(int streamId, std::uint64_t length);
  std::uint64_t nextMarketDataSequence(const std::string& instrumentId);

  FramePublisher& publisher_;
  ContractContext context_;
  std::map<std::string, std::uint64_t> marketDataSequences_;
  std::size_t bufferSize_{16384};
  std::unique_ptr<char[]> buffer_{new char[bufferSize_]};
};

}  // namespace ors::ctp

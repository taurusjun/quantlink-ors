// ContractSink tests: SBE template, stream ownership, execution sequence,
// 20-level market data and VenueSession high-water encoding.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "com_quantlink_contract_sbe/MarketData.h"
#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/OrderUpdate.h"
#include "com_quantlink_contract_sbe/Trade.h"
#include "com_quantlink_contract_sbe/VenueSession.h"
#include "ors/ctp/ContractSink.h"

using SbeOrderUpdate = com::quantlink::contract::sbe::OrderUpdate;
using SbeTrade = com::quantlink::contract::sbe::Trade;
using SbeMarketData = com::quantlink::contract::sbe::MarketData;
using SbeVenueSession = com::quantlink::contract::sbe::VenueSession;
using SbeVenueSessionState = com::quantlink::contract::sbe::VenueSessionState;

namespace {

using namespace ors::ctp;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

struct Frame {
  int streamId{0};
  std::vector<char> bytes;
};

class FakePublisher final : public FramePublisher {
 public:
  bool publish(int streamId, const char* data, std::size_t length) override {
    Frame frame;
    frame.streamId = streamId;
    frame.bytes.assign(data, data + length);
    frames.push_back(std::move(frame));
    return true;
  }

  std::vector<Frame> frames;
};

ContractContext context() {
  ContractContext value;
  value.venueId = "SIMNOW";
  value.accountId = "ACC";
  value.orsId = "ors-ctp-1";
  value.environment = "SHADOW";
  value.sourceSessionId = 7;
  value.sourcePublicationEpoch = 1;
  return value;
}

std::uint16_t templateId(const Frame& frame) {
  com::quantlink::contract::sbe::MessageHeader header;
  header.wrap(const_cast<char*>(frame.bytes.data()), 0, 0, frame.bytes.size());
  return header.templateId();
}

std::string fieldText(const char* data, std::size_t maxBytes) {
  std::size_t length = 0;
  while (length < maxBytes && data[length] != '\0') {
    ++length;
  }
  return std::string(data, length);
}

void checkExecution() {
  FakePublisher publisher;
  ContractSink sink(publisher, context());
  OrderUpdate update;
  update.orderRef = 42;
  update.exchangeOrderId = "EX-42";
  update.status = OrderStatus::Ack;
  update.cancelState = CancelState::None;
  update.filledQty = 0;
  update.remainingQty = 1;
  update.avgPrice = 0;
  update.reason = "ok";
  update.errorCode = 0;
  update.tradingDay = "20261124";
  update.exchangeTimestamp = 11;
  update.ingestTimestamp = 12;
  update.requestId = "req-42";
  update.sequence = 1;
  update.side = Side::Buy;
  update.offsetFlag = Offset::Open;
  update.orderQty = 1;
  update.price = 777.5;
  update.exchange = "SHFE";

  Trade trade;
  trade.tradeId = "T-42";
  trade.orderRef = 42;
  trade.exchangeOrderId = "EX-42";
  trade.sequence = 2;
  trade.instrumentId = "au2612";
  trade.exchange = "SHFE";
  trade.side = Side::Buy;
  trade.offsetFlag = Offset::Open;
  trade.price = 777.5;
  trade.qty = 1;
  trade.exchangeTimestamp = 13;
  trade.ingestTimestamp = 14;
  trade.tradingDay = "20261124";
  trade.requestId = "req-42";

  check(sink.publishOrderUpdate(update), "OrderUpdate published");
  check(sink.publishTrade(trade), "Trade published");
  check(publisher.frames.size() == 2, "two execution frames");
  check(publisher.frames[0].streamId == 2003, "OrderUpdate uses execution stream");
  check(publisher.frames[1].streamId == 2003, "Trade shares execution stream");
  check(templateId(publisher.frames[0]) ==
            SbeOrderUpdate::sbeTemplateId(),
        "OrderUpdate template id");
  check(templateId(publisher.frames[1]) ==
            SbeTrade::sbeTemplateId(),
        "Trade template id");
}

void checkMarketData() {
  FakePublisher publisher;
  ContractSink sink(publisher, context());
  MarketData data;
  data.instrumentId = "au2612";
  data.exchange = "SHFE";
  data.lastPrice = 777.5;
  data.exchangeTimestamp = 111;
  data.ingestTimestamp = 222;
  data.volume = 100;
  data.turnover = 1000;
  data.openInterest = 10;
  data.bidPrices.fill(777.0);
  data.bidQtys.fill(1);
  data.askPrices.fill(778.0);
  data.askQtys.fill(2);

  check(sink.publishMarketData(data), "first MarketData published");
  check(sink.publishMarketData(data), "second MarketData published");
  check(publisher.frames.size() == 2, "two market data frames");
  check(publisher.frames[0].streamId == 1001, "MarketData stream");
  check(publisher.frames[1].streamId == 1001, "MarketData stream repeated");

  com::quantlink::contract::sbe::MessageHeader header;
  header.wrap(publisher.frames[0].bytes.data(), 0, 0, publisher.frames[0].bytes.size());
  SbeMarketData decoded;
  decoded.wrapForDecode(publisher.frames[0].bytes.data(),
                        com::quantlink::contract::sbe::MessageHeader::encodedLength(),
                        header.blockLength(), header.version(),
                        publisher.frames[0].bytes.size());
  check(decoded.seq() == 1, "first market data seq is 1");
  check(decoded.exchangeTimestamp() == 111, "exchange timestamp reaches SBE");
  check(decoded.ingestTimestamp() == 222, "ingest timestamp reaches SBE");
  auto& bids = decoded.bids();
  check(bids.count() == 20, "bid depth is 20");
  for (int level = 0; level < 20; ++level) {
    auto& entry = bids.next();
    if (level == 0) check(entry.price() == 777.0, "bid level 1 price");
  }
  auto& asks = decoded.asks();
  check(asks.count() == 20, "ask depth is 20");
}

void checkSession() {
  FakePublisher publisher;
  ContractSink sink(publisher, context());
  VenueSessionEvent event;
  event.tradingDay = "20261124";
  event.loginState = com::quantlink::contract::sbe::GatewayState::UP;
  event.settlementState = com::quantlink::contract::sbe::SettlementState::SETTLED;
  event.adapterSessionId = "front:session";
  event.state = SbeVenueSessionState::READY;
  event.snapshotTimestamp = 100;
  event.lastSeqByStream = {{2003, 9}, {1001, 4}};

  check(sink.publishSession(event), "VenueSession published");
  check(publisher.frames.size() == 1, "one session frame");
  check(publisher.frames[0].streamId == 2006, "session stream");
  check(templateId(publisher.frames[0]) ==
            SbeVenueSession::sbeTemplateId(),
        "session template id");

  com::quantlink::contract::sbe::MessageHeader header;
  header.wrap(publisher.frames[0].bytes.data(), 0, 0, publisher.frames[0].bytes.size());
  SbeVenueSession decoded;
  decoded.wrapForDecode(publisher.frames[0].bytes.data(),
                        com::quantlink::contract::sbe::MessageHeader::encodedLength(),
                        header.blockLength(), header.version(),
                        publisher.frames[0].bytes.size());
  auto& sequences = decoded.lastSeqByStream();
  check(sequences.count() == 2, "two high-water entries");
  bool executionFound = false;
  for (int index = 0; index < 2; ++index) {
    auto& entry = sequences.next();
    if (entry.streamId() == 2003 && entry.lastSeq() == 9) executionFound = true;
  }
  check(executionFound, "execution high-water encoded");
}

void checkSessionEpochUpdate() {
  FakePublisher publisher;
  ContractSink sink(publisher, context());
  sink.setSourcePublicationEpoch(9);
  VenueSessionEvent event;
  event.tradingDay = "20261124";
  event.loginState = com::quantlink::contract::sbe::GatewayState::UP;
  event.settlementState = com::quantlink::contract::sbe::SettlementState::SETTLED;
  event.state = SbeVenueSessionState::LOST;
  event.snapshotTimestamp = 100;
  event.lastSeqByStream = {{2003, 1}};
  check(sink.publishSession(event), "epoch-updated session published");

  com::quantlink::contract::sbe::MessageHeader header;
  header.wrap(publisher.frames[0].bytes.data(), 0, 0, publisher.frames[0].bytes.size());
  SbeVenueSession decoded;
  decoded.wrapForDecode(publisher.frames[0].bytes.data(),
                        com::quantlink::contract::sbe::MessageHeader::encodedLength(),
                        header.blockLength(), header.version(),
                        publisher.frames[0].bytes.size());
  check(decoded.sourcePublicationEpoch() == 9, "session epoch setter reaches SBE");
}

void checkFittedSessionFields() {
  FakePublisher publisher;
  ContractContext value = context();
  value.venueId = std::string(40, 'V');
  value.orsId = std::string(40, 'O');
  value.environment = std::string(20, 'E');
  ContractSink sink(publisher, value);
  VenueSessionEvent event;
  event.tradingDay = std::string(12, 'D');
  event.loginState = com::quantlink::contract::sbe::GatewayState::UP;
  event.settlementState = com::quantlink::contract::sbe::SettlementState::SETTLED;
  event.adapterSessionId = std::string(100, 'A');
  event.state = SbeVenueSessionState::READY;
  event.reason = std::string(100, 'R');
  event.snapshotTimestamp = 100;
  event.lastSeqByStream = {{2003, 1}};
  check(sink.publishSession(event), "over-width session published");

  com::quantlink::contract::sbe::MessageHeader header;
  header.wrap(publisher.frames[0].bytes.data(), 0, 0, publisher.frames[0].bytes.size());
  SbeVenueSession decoded;
  decoded.wrapForDecode(publisher.frames[0].bytes.data(),
                        com::quantlink::contract::sbe::MessageHeader::encodedLength(),
                        header.blockLength(), header.version(),
                        publisher.frames[0].bytes.size());
  check(fieldText(decoded.venueId(), 32).size() == 32, "venueId fits 32 bytes");
  check(fieldText(decoded.orsId(), 32).size() == 32, "orsId fits 32 bytes");
  check(fieldText(decoded.environment(), 16).size() == 16,
        "environment fits 16 bytes");
  check(fieldText(decoded.tradingDay(), 8).size() == 8, "tradingDay fits 8 bytes");
  check(fieldText(decoded.adapterSessionId(), 64).size() == 64,
        "adapterSessionId fits 64 bytes");
  check(fieldText(decoded.reason(), 64).size() == 64, "reason fits 64 bytes");
}

void checkSnapshots() {
  FakePublisher publisher;
  ContractSink sink(publisher, context());
  SnapshotHeader header;
  header.accountId = "ACC";
  header.tradingDay = "20261124";
  header.snapshotId = "snap-1";
  header.snapshotTimestamp = 100;
  header.staleAfterMs = 5000;

  AccountSnapshot account;
  account.header = header;
  PositionSnapshot position;
  position.header = header;
  position.instrumentId = "au2612";
  position.exchange = "SHFE";

  check(sink.publishAccountSnapshot(account), "AccountSnapshot published");
  check(sink.publishPositionSnapshot(position), "PositionSnapshot published");
  check(publisher.frames.size() == 2, "two snapshot frames");
  check(publisher.frames[0].streamId == 2005, "account snapshot stream");
  check(publisher.frames[1].streamId == 2004, "position snapshot stream");
}

}  // namespace

int main() {
  checkExecution();
  checkMarketData();
  checkSession();
  checkSessionEpochUpdate();
  checkFittedSessionFields();
  checkSnapshots();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "contract sink tests passed\n";
  return 0;
}

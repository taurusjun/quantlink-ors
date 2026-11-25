#include "ors/ctp/ContractSink.h"

#include <string>
#include <utility>

#include "com_quantlink_contract_sbe/AccountSnapshot.h"
#include "com_quantlink_contract_sbe/CancelState.h"
#include "com_quantlink_contract_sbe/DataSource.h"
#include "com_quantlink_contract_sbe/MarketData.h"
#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/OrderStatus.h"
#include "com_quantlink_contract_sbe/OrderUpdate.h"
#include "com_quantlink_contract_sbe/PositionSnapshot.h"
#include "com_quantlink_contract_sbe/Side.h"
#include "com_quantlink_contract_sbe/Trade.h"
#include "com_quantlink_contract_sbe/VenueSession.h"

namespace ors::ctp {
namespace sbe = com::quantlink::contract::sbe;
ContractSink::ContractSink(FramePublisher& publisher, ContractContext context)
    : publisher_(publisher), context_(std::move(context)) {}

bool ContractSink::publishOrderUpdate(const OrderUpdate& update) {
  sbe::OrderUpdate message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .orderRef(update.orderRef)
      .putExchangeOrderId(update.exchangeOrderId)
      .status(static_cast<sbe::OrderStatus::Value>(update.status))
      .cancelState(static_cast<sbe::CancelState::Value>(update.cancelState))
      .filledQty(update.filledQty)
      .remainingQty(update.remainingQty)
      .avgPrice(update.avgPrice)
      .putReason(update.reason)
      .errorCode(update.errorCode)
      .putTradingDay(update.tradingDay)
      .exchangeTimestamp(update.exchangeTimestamp)
      .ingestTimestamp(update.ingestTimestamp)
      .putRequestId(update.requestId)
      .sequence(update.sequence)
      .side(static_cast<sbe::Side::Value>(update.side))
      .offsetFlag(static_cast<sbe::Offset::Value>(update.offsetFlag))
      .orderQty(update.orderQty)
      .price(update.price)
      .putAccountId(context_.accountId)
      .putVenueId(context_.venueId)
      .putExchange(update.exchange)
      .putEnvironment(context_.environment)
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch);
  return publishFrame(context_.streams.execution, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishTrade(const Trade& trade) {
  sbe::Trade message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putTradeId(trade.tradeId)
      .orderRef(trade.orderRef)
      .putExchangeOrderId(trade.exchangeOrderId)
      .sequence(trade.sequence)
      .putInstrumentId(trade.instrumentId)
      .side(static_cast<sbe::Side::Value>(trade.side))
      .offsetFlag(static_cast<sbe::Offset::Value>(trade.offsetFlag))
      .price(trade.price)
      .qty(trade.qty)
      .exchangeTimestamp(trade.exchangeTimestamp)
      .ingestTimestamp(trade.ingestTimestamp)
      .putTradingDay(trade.tradingDay)
      .fee(0.0)
      .feeKnown(0)
      .putRequestId(trade.requestId)
      .putAccountId(context_.accountId)
      .putVenueId(context_.venueId)
      .putExchange(trade.exchange)
      .putEnvironment(context_.environment)
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch);
  return publishFrame(context_.streams.execution, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishMarketData(const MarketData& data) {
  sbe::MarketData message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putInstrumentId(data.instrumentId)
      .putVenueId(context_.venueId)
      .putExchange(data.exchange)
      .putEnvironment(context_.environment)
      .exchangeTimestamp(0)
      .ingestTimestamp(0)
      .lastPrice(data.lastPrice)
      .lastTradeQty(0)
      .lastTradeTime(0)
      .volume(data.volume)
      .turnover(data.turnover)
      .openInterest(data.openInterest)
      .synthetic(0)
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch)
      .seq(nextMarketDataSequence(data.instrumentId));
  auto& bids = message.bidsCount(kMarketDepthLevels);
  for (std::size_t level = 0; level < kMarketDepthLevels; ++level) {
    bids.next().price(data.bidPrices[level]).qty(data.bidQtys[level]);
  }
  auto& asks = message.asksCount(kMarketDepthLevels);
  for (std::size_t level = 0; level < kMarketDepthLevels; ++level) {
    asks.next().price(data.askPrices[level]).qty(data.askQtys[level]);
  }
  return publishFrame(context_.streams.marketData, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishAccountSnapshot(const AccountSnapshot& snapshot) {
  sbe::AccountSnapshot message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putAccountId(context_.accountId)
      .balance(snapshot.balance)
      .available(snapshot.available)
      .margin(snapshot.margin)
      .frozenMargin(snapshot.frozenMargin)
      .commission(snapshot.commission)
      .closeProfit(snapshot.closeProfit)
      .positionProfit(snapshot.positionProfit)
      .deposit(snapshot.deposit)
      .withdraw(snapshot.withdraw)
      .putVenueId(context_.venueId)
      .putEnvironment(context_.environment)
      .putSnapshotId(snapshot.header.snapshotId)
      .source(sbe::DataSource::CTP)
      .putTradingDay(snapshot.header.tradingDay)
      .staleAfterMs(snapshot.header.staleAfterMs)
      .snapshotTimestamp(snapshot.header.snapshotTimestamp);
  return publishFrame(context_.streams.account, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishPositionSnapshot(const PositionSnapshot& snapshot) {
  sbe::PositionSnapshot message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putAccountId(context_.accountId)
      .putInstrumentId(snapshot.instrumentId)
      .longQty(snapshot.longQty)
      .shortQty(snapshot.shortQty)
      .todayLong(snapshot.todayLong)
      .todayShort(snapshot.todayShort)
      .yesterdayLong(snapshot.yesterdayLong)
      .yesterdayShort(snapshot.yesterdayShort)
      .avgPrice(snapshot.avgPrice)
      .longAvgPrice(snapshot.longAvgPrice)
      .shortAvgPrice(snapshot.shortAvgPrice)
      .settlementPrice(snapshot.settlementPrice)
      .putExchange(snapshot.exchange)
      .putVenueId(context_.venueId)
      .putEnvironment(context_.environment)
      .putTradingDay(snapshot.header.tradingDay)
      .putSnapshotId(snapshot.header.snapshotId)
      .source(sbe::DataSource::CTP)
      .staleAfterMs(snapshot.header.staleAfterMs)
      .snapshotTimestamp(snapshot.header.snapshotTimestamp);
  return publishFrame(context_.streams.position, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

void ContractSink::setSourcePublicationEpoch(std::uint64_t epoch) {
  context_.sourcePublicationEpoch = epoch;
}

bool ContractSink::publishSession(const VenueSessionEvent& session) {
  sbe::VenueSession message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putVenueId(context_.venueId)
      .putAccountId(context_.accountId)
      .putOrsId(context_.orsId)
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch)
      .putEnvironment(context_.environment)
      .putTradingDay(session.tradingDay)
      .loginState(session.loginState)
      .settlementState(session.settlementState)
      .putAdapterSessionId(session.adapterSessionId)
      .state(session.state)
      .putReason(session.reason)
      .snapshotTimestamp(session.snapshotTimestamp);
  auto& sequences = message.lastSeqByStreamCount(
      static_cast<std::uint16_t>(session.lastSeqByStream.size()));
  for (const auto& [streamId, lastSeq] : session.lastSeqByStream) {
    sequences.next().streamId(streamId).lastSeq(lastSeq);
  }
  return publishFrame(context_.streams.session, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishFrame(int streamId, std::uint64_t length) {
  return publisher_.publish(streamId, buffer_.get(), static_cast<std::size_t>(length));
}

std::uint64_t ContractSink::nextMarketDataSequence(const std::string& instrumentId) {
  return ++marketDataSequences_[instrumentId];
}

}  // namespace ors::ctp

#include "ors/ctp/ContractSink.h"

#include <string>
#include <utility>

#include "ors/base/ContractText.h"

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
using base::fitContractField;
ContractSink::ContractSink(FramePublisher& publisher, ContractContext context)
    : publisher_(publisher), context_(std::move(context)) {}

bool ContractSink::publishOrderUpdate(const OrderUpdate& update) {
  sbe::OrderUpdate message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .orderRef(update.orderRef)
      .putExchangeOrderId(fitContractField(update.exchangeOrderId, 32))
      .status(static_cast<sbe::OrderStatus::Value>(update.status))
      .cancelState(static_cast<sbe::CancelState::Value>(update.cancelState))
      .filledQty(update.filledQty)
      .remainingQty(update.remainingQty)
      .avgPrice(update.avgPrice)
      .putReason(fitContractField(update.reason, 64))
      .errorCode(update.errorCode)
      .putTradingDay(fitContractField(update.tradingDay, 8))
      .exchangeTimestamp(update.exchangeTimestamp)
      .ingestTimestamp(update.ingestTimestamp)
      .putRequestId(fitContractField(update.requestId, 64))
      .sequence(update.sequence)
      .side(static_cast<sbe::Side::Value>(update.side))
      .offsetFlag(static_cast<sbe::Offset::Value>(update.offsetFlag))
      .orderQty(update.orderQty)
      .price(update.price)
      .putAccountId(fitContractField(context_.accountId, 16))
      .putVenueId(fitContractField(context_.venueId, 32))
      .putExchange(fitContractField(update.exchange, 8))
      .putEnvironment(fitContractField(context_.environment, 16))
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch);
  return publishFrame(context_.streams.execution, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishTrade(const Trade& trade) {
  sbe::Trade message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putTradeId(fitContractField(trade.tradeId, 32))
      .orderRef(trade.orderRef)
      .putExchangeOrderId(fitContractField(trade.exchangeOrderId, 32))
      .sequence(trade.sequence)
      .putInstrumentId(fitContractField(trade.instrumentId, 32))
      .side(static_cast<sbe::Side::Value>(trade.side))
      .offsetFlag(static_cast<sbe::Offset::Value>(trade.offsetFlag))
      .price(trade.price)
      .qty(trade.qty)
      .exchangeTimestamp(trade.exchangeTimestamp)
      .ingestTimestamp(trade.ingestTimestamp)
      .putTradingDay(fitContractField(trade.tradingDay, 8))
      .fee(0.0)
      .feeKnown(0)
      .putRequestId(fitContractField(trade.requestId, 64))
      .putAccountId(fitContractField(context_.accountId, 16))
      .putVenueId(fitContractField(context_.venueId, 32))
      .putExchange(fitContractField(trade.exchange, 8))
      .putEnvironment(fitContractField(context_.environment, 16))
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch);
  return publishFrame(context_.streams.execution, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishMarketData(const MarketData& data) {
  sbe::MarketData message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putInstrumentId(fitContractField(data.instrumentId, 32))
      .putVenueId(fitContractField(context_.venueId, 32))
      .putExchange(fitContractField(data.exchange, 8))
      .putEnvironment(fitContractField(context_.environment, 16))
      .exchangeTimestamp(data.exchangeTimestamp)
      .ingestTimestamp(data.ingestTimestamp)
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
      .putAccountId(fitContractField(context_.accountId, 16))
      .balance(snapshot.balance)
      .available(snapshot.available)
      .margin(snapshot.margin)
      .frozenMargin(snapshot.frozenMargin)
      .commission(snapshot.commission)
      .closeProfit(snapshot.closeProfit)
      .positionProfit(snapshot.positionProfit)
      .deposit(snapshot.deposit)
      .withdraw(snapshot.withdraw)
      .putVenueId(fitContractField(context_.venueId, 32))
      .putEnvironment(fitContractField(context_.environment, 16))
      .putSnapshotId(fitContractField(snapshot.header.snapshotId, 64))
      .source(sbe::DataSource::CTP)
      .putTradingDay(fitContractField(snapshot.header.tradingDay, 8))
      .staleAfterMs(snapshot.header.staleAfterMs)
      .snapshotTimestamp(snapshot.header.snapshotTimestamp);
  return publishFrame(context_.streams.account, message.encodedLength() + sbe::MessageHeader::encodedLength());
}

bool ContractSink::publishPositionSnapshot(const PositionSnapshot& snapshot) {
  sbe::PositionSnapshot message;
  message.wrapAndApplyHeader(buffer_.get(), 0, bufferSize_)
      .putAccountId(fitContractField(context_.accountId, 16))
      .putInstrumentId(fitContractField(snapshot.instrumentId, 32))
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
      .putExchange(fitContractField(snapshot.exchange, 8))
      .putVenueId(fitContractField(context_.venueId, 32))
      .putEnvironment(fitContractField(context_.environment, 16))
      .putTradingDay(fitContractField(snapshot.header.tradingDay, 8))
      .putSnapshotId(fitContractField(snapshot.header.snapshotId, 64))
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
      .putVenueId(fitContractField(context_.venueId, 32))
      .putAccountId(fitContractField(context_.accountId, 16))
      .putOrsId(fitContractField(context_.orsId, 32))
      .sourceSessionId(context_.sourceSessionId)
      .sourcePublicationEpoch(context_.sourcePublicationEpoch)
      .putEnvironment(fitContractField(context_.environment, 16))
      .putTradingDay(fitContractField(session.tradingDay, 8))
      .loginState(session.loginState)
      .settlementState(session.settlementState)
      .putAdapterSessionId(fitContractField(session.adapterSessionId, 64))
      .state(session.state)
      .putReason(fitContractField(session.reason, 64))
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

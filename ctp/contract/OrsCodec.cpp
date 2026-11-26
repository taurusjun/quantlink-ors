#include "contract/OrsCodec.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include "com_quantlink_contract_sbe/MessageHeader.h"

namespace ors {
namespace {

namespace sbe = com::quantlink::contract::sbe;

std::string text(const char* value, std::size_t length) {
  if (value == nullptr) {
    return {};
  }
  std::size_t actual = 0;
  while (actual < length && value[actual] != '\0') {
    ++actual;
  }
  while (actual > 0 && value[actual - 1] == ' ') {
    --actual;
  }
  return std::string(value, actual);
}

template <std::size_t N>
void copyText(char (&destination)[N], std::string_view value) {
  std::memset(destination, 0, N);
  const std::size_t length = std::min(value.size(), N - 1);
  std::memcpy(destination, value.data(), length);
}

}  // namespace

illuminati::infra::RequestMsg decodeRequest(sbe::OrsRequest& decoder) {
  illuminati::infra::RequestMsg request{};
  const auto description = decoder.contractDescription();
  copyText(request.Contract_Description.InstrumentName,
           text(description.instrumentName(), 32));
  copyText(request.Contract_Description.Symbol,
           text(description.symbol(), 50));
  request.Contract_Description.ExpiryDate = description.expiryDate();
  request.Contract_Description.StrikePrice = description.strikePrice();
  std::memcpy(request.Contract_Description.OptionType,
              description.optionType(), 2);
  request.Contract_Description.CALevel = description.caLevel();
  request.Request_Type =
      static_cast<illuminati::infra::RequestType>(decoder.requestType());
  request.OrdType =
      static_cast<illuminati::infra::OrderType>(decoder.orderType());
  request.Duration =
      static_cast<illuminati::infra::OrderDuration>(decoder.duration());
  request.PxType =
      static_cast<illuminati::infra::PriceType>(decoder.priceType());
  request.PosDirection =
      static_cast<illuminati::infra::PositionDirection>(decoder.positionDirection());
  request.OrderID = decoder.orderId();
  request.Token = decoder.token();
  request.Quantity = decoder.quantity();
  request.QuantityFilled = decoder.quantityFilled();
  request.DisclosedQnty = decoder.disclosedQty();
  request.Price = decoder.price();
  request.TimeStamp = decoder.timestamp();
  copyText(request.AccountID, text(decoder.accountId(), 11));
  request.Transaction_Type = static_cast<unsigned char>(decoder.transactionType());
  request.Exchange_Type = decoder.exchangeType();
  return request;
}

std::size_t encodeResponse(const illuminati::infra::ResponseMsg& response,
                           char* destination, std::size_t capacity) {
  sbe::OrsResponse encoder;
  encoder.wrapAndApplyHeader(destination, 0, capacity)
      .responseType(static_cast<sbe::ResponseType::Value>(response.Response_Type))
      .childResponse(
          static_cast<sbe::SubResponseType::Value>(response.Child_Response))
      .orderId(response.OrderID)
      .errorCode(response.ErrorCode)
      .quantity(response.Quantity)
      .price(response.Price)
      .timestamp(response.TimeStamp)
      .side(static_cast<std::uint8_t>(response.Side))
      .putSymbol(std::string_view(response.Symbol))
      .putAccountId(std::string_view(response.AccountID))
      .exchangeOrderId(response.ExchangeOrderId);
  return encoder.encodedLength() + sbe::MessageHeader::encodedLength();
}

std::size_t encodeMarketUpdate(const illuminati::md::MarketUpdateNew& update,
                               char* destination, std::size_t capacity) {
  sbe::MarketUpdate encoder;
  encoder.wrapAndApplyHeader(destination, 0, capacity)
      .putSymbol(std::string_view(update.m_symbol))
      .exchangeName(update.m_exchangeName)
      .exchangeTimestamp(update.m_exchTS)
      .localTimestamp(update.m_timestamp)
      .sequence(update.m_seqnum)
      .reportSequence(update.m_rptseqnum)
      .tokenId(update.m_tokenId)
      .lastPrice(update.m_newPrice)
      .lastTradedPrice(update.m_lastTradedPrice)
      .lastTradedTime(update.m_lastTradedTime)
      .lastTradedQty(update.m_lastTradedQuantity)
      .totalTradedValue(update.m_totalTradedValue)
      .totalTradedQuantity(update.m_totalTradedQuantity)
      .yieldRate(update.m_yield)
      .openInterest(0.0)
      .updateType(update.m_updateType)
      .feedType(update.m_feedType)
      .side(update.m_side)
      .validBids(update.m_validBids)
      .validAsks(update.m_validAsks);

  auto& bids = encoder.bidsCount(20);
  for (std::size_t level = 0; level < 20; ++level) {
    bids.next()
        .price(update.m_bidUpdates[level].price)
        .qty(update.m_bidUpdates[level].quantity)
        .orderCount(update.m_bidUpdates[level].orderCount);
  }
  auto& asks = encoder.asksCount(20);
  for (std::size_t level = 0; level < 20; ++level) {
    asks.next()
        .price(update.m_askUpdates[level].price)
        .qty(update.m_askUpdates[level].quantity)
        .orderCount(update.m_askUpdates[level].orderCount);
  }
  return encoder.encodedLength() + sbe::MessageHeader::encodedLength();
}

AccountQueryRequest decodeAccountQuery(sbe::AccountQuery& decoder) {
  AccountQueryRequest request;
  request.requestId = decoder.requestId();
  request.accountId = text(decoder.accountId(), 11);
  request.timestamp = decoder.timestamp();
  return request;
}

std::size_t encodeAccountSnapshot(const AccountSnapshot& snapshot,
                                  char* destination, std::size_t capacity) {
  sbe::AccountSnapshot encoder;
  encoder.wrapAndApplyHeader(destination, 0, capacity)
      .requestId(snapshot.requestId)
      .putAccountId(std::string_view(snapshot.accountId))
      .putTradingDay(std::string_view(snapshot.tradingDay))
      .balance(snapshot.balance)
      .available(snapshot.available)
      .margin(snapshot.margin)
      .frozenMargin(snapshot.frozenMargin)
      .commission(snapshot.commission)
      .closeProfit(snapshot.closeProfit)
      .positionProfit(snapshot.positionProfit)
      .deposit(snapshot.deposit)
      .withdraw(snapshot.withdraw)
      .errorCode(snapshot.errorCode)
      .putErrorMsg(std::string_view(snapshot.errorMsg))
      .timestamp(snapshot.timestamp);
  return encoder.encodedLength() + sbe::MessageHeader::encodedLength();
}

ClientRegisterRequest decodeClientRegister(
    sbe::ClientRegister& decoder) {
  ClientRegisterRequest request;
  request.requestId = decoder.requestId();
  request.instanceId = text(decoder.instanceId(), 64);
  request.accountId = text(decoder.accountId(), 11);
  request.environment = text(decoder.environment(), 16);
  request.timestamp = decoder.timestamp();
  return request;
}

std::size_t encodeClientRegisterResponse(const ClientRegisterResult& result,
                                         char* destination,
                                         std::size_t capacity) {
  sbe::ClientRegisterResponse encoder;
  encoder.wrapAndApplyHeader(destination, 0, capacity)
      .requestId(result.requestId)
      .clientId(result.clientId)
      .errorCode(result.errorCode)
      .putErrorMsg(std::string_view(result.errorMsg))
      .timestamp(result.timestamp);
  return encoder.encodedLength() + sbe::MessageHeader::encodedLength();
}

}  // namespace ors

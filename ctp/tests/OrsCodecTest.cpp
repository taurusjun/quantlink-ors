#include <cstring>
#include <iostream>
#include <string>

#include "com_quantlink_contract_sbe/AccountQuery.h"
#include "com_quantlink_contract_sbe/AccountSnapshot.h"
#include "com_quantlink_contract_sbe/ClientRegisterResponse.h"
#include "com_quantlink_contract_sbe/MessageHeader.h"
#include "com_quantlink_contract_sbe/OrsRequest.h"
#include "com_quantlink_contract_sbe/OrsResponse.h"
#include "contract/OrsCodec.h"

int main() {
  using namespace com::quantlink::contract::sbe;
  char buffer[8192];

  OrsRequest request;
  request.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
      .contractDescription()
      .putInstrumentName("au2612")
      .putSymbol("au2612")
      .expiryDate(0)
      .strikePrice(0)
      .putOptionType(std::string_view("XX"))
      .caLevel(0);
  request.requestType(RequestType::NEW_ORDER)
      .orderType(OrderType::LIMIT)
      .duration(OrderDuration::DAY)
      .priceType(PriceType::PER_UNIT)
      .positionDirection(PositionDirection::OPEN)
      .orderId(42)
      .token(7)
      .quantity(2)
      .quantityFilled(0)
      .disclosedQty(2)
      .price(735.5)
      .timestamp(123)
      .putAccountId("142266")
      .transactionType(TransactionType::BUY)
      .exchangeType(57);

  MessageHeader requestHeader;
  requestHeader.wrap(buffer, 0, 0, sizeof(buffer));
  OrsRequest requestDecoder;
  requestDecoder.wrapForDecode(buffer, MessageHeader::encodedLength(), requestHeader.blockLength(), requestHeader.version(), sizeof(buffer));
  const auto decodedRequest = ors::decodeRequest(requestDecoder);
  if (decodedRequest.OrderID != 42 || decodedRequest.Quantity != 2 ||
      decodedRequest.Price != 735.5 ||
      std::string(decodedRequest.Contract_Description.Symbol) != "au2612") {
    std::cerr << "request mapping failed\n";
    return 1;
  }

  illuminati::infra::ResponseMsg response;
  response.Response_Type = illuminati::infra::CANCEL_ORDER_REJECT;
  response.OrderID = 42;
  response.ErrorCode = 26;
  response.Quantity = 1;
  response.Price = 735.5;
  response.Side = 'B';
  std::strcpy(response.Symbol, "au2612");
  std::strcpy(response.AccountID, "142266");
  response.ExchangeOrderId = 1.25;

  const std::size_t responseLength =
      ors::encodeResponse(response, buffer, sizeof(buffer));
  MessageHeader responseHeader;
  responseHeader.wrap(buffer, 0, 0, sizeof(buffer));
  OrsResponse responseDecoder;
  responseDecoder.wrapForDecode(buffer, MessageHeader::encodedLength(), responseHeader.blockLength(), responseHeader.version(), sizeof(buffer));
  if (responseLength == 0 || responseDecoder.responseType() !=
                                 ResponseType::CANCEL_ORDER_REJECT ||
      responseDecoder.orderId() != 42 || responseDecoder.exchangeOrderId() != 1.25) {
    std::cerr << "response mapping failed\n";
    return 1;
  }

  AccountQuery query;
  query.wrapAndApplyHeader(buffer, 0, sizeof(buffer))
      .requestId(9)
      .putAccountId("142266")
      .timestamp(123);
  MessageHeader queryHeader;
  queryHeader.wrap(buffer, 0, 0, sizeof(buffer));
  AccountQuery queryDecoder;
  queryDecoder.wrapForDecode(buffer, MessageHeader::encodedLength(), queryHeader.blockLength(), queryHeader.version(), sizeof(buffer));
  const auto decodedQuery = ors::decodeAccountQuery(queryDecoder);
  if (decodedQuery.requestId != 9 || decodedQuery.accountId != "142266") {
    std::cerr << "account query mapping failed\n";
    return 1;
  }

  ors::AccountSnapshot snapshot;
  snapshot.requestId = 9;
  snapshot.accountId = "142266";
  snapshot.tradingDay = "20261126";
  snapshot.balance = 1000;
  snapshot.available = 900;
  snapshot.errorCode = 0;
  snapshot.timestamp = 123;
  const std::size_t snapshotLength =
      ors::encodeAccountSnapshot(snapshot, buffer, sizeof(buffer));
  MessageHeader snapshotHeader;
  snapshotHeader.wrap(buffer, 0, 0, sizeof(buffer));
  AccountSnapshot snapshotDecoder;
  snapshotDecoder.wrapForDecode(buffer, MessageHeader::encodedLength(), snapshotHeader.blockLength(), snapshotHeader.version(), sizeof(buffer));
  if (snapshotLength == 0 || snapshotDecoder.requestId() != 9 ||
      snapshotDecoder.balance() != 1000) {
    std::cerr << "account snapshot mapping failed\n";
    return 1;
  }

  ors::ClientRegisterResult result;
  result.requestId = 10;
  result.clientId = 1;
  result.timestamp = 123;
  const std::size_t registerLength =
      ors::encodeClientRegisterResponse(result, buffer, sizeof(buffer));
  MessageHeader registerHeader;
  registerHeader.wrap(buffer, 0, 0, sizeof(buffer));
  ClientRegisterResponse registerDecoder;
  registerDecoder.wrapForDecode(buffer, MessageHeader::encodedLength(), registerHeader.blockLength(), registerHeader.version(), sizeof(buffer));
  if (registerLength == 0 || registerDecoder.requestId() != 10 ||
      registerDecoder.clientId() != 1) {
    std::cerr << "client register mapping failed\n";
    return 1;
  }

  return 0;
}

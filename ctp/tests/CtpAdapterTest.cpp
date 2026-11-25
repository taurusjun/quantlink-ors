// CtpAdapter tests: request mapping, cancel addressing, execution reports and
// the modify rejection. The sender is faked and rows are built directly, so
// nothing here logs in or sends anything to SimNow.

#include <ThostFtdcTraderApi.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "ors/ctp/CtpAdapter.h"

namespace {

using ors::ctp::CancelRequest;
using ors::ctp::CancelState;
using ors::ctp::CtpAdapter;
using ors::ctp::CtpConfig;
using ors::ctp::CtpTraderSession;
using ors::ctp::ExecutionSink;
using ors::ctp::kModifyUnsupportedReason;
using ors::ctp::ModifyRequest;
using ors::ctp::Offset;
using ors::ctp::OrderRequest;
using ors::ctp::OrderSender;
using ors::ctp::OrderStatus;
using ors::ctp::OrderType;
using ors::ctp::OrderUpdate;
using ors::ctp::Side;
using ors::ctp::TimeInForce;
using ors::ctp::Trade;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

void checkNear(double actual, double expected, const std::string& what) {
  check(actual > expected - 1e-9 && actual < expected + 1e-9, what);
}

template <typename Row>
Row zeroed() {
  Row row;
  std::memset(&row, 0, sizeof(row));
  return row;
}

template <std::size_t N>
void setText(char (&field)[N], const std::string& value) {
  std::snprintf(field, N, "%s", value.c_str());
}

template <std::size_t N>
std::string fieldText(const char (&field)[N]) {
  std::size_t length = 0;
  while (length < N && field[length] != '\0') {
    ++length;
  }
  return std::string(field, length);
}

CtpConfig makeConfig() {
  CtpConfig config;
  config.brokerId = "9999";
  config.investorId = "900001";
  config.password = "not-used";
  config.appId = "not-used";
  config.authCode = "not-used";
  config.tradeFront = "tcp://127.0.0.1:30001";
  return config;
}

struct FakeSender final : OrderSender {
  int insertResult{0};
  int cancelResult{0};
  std::string error{"sender failure"};
  int inserts{0};
  int cancels{0};
  CThostFtdcInputOrderField lastInsert{};
  CThostFtdcInputOrderActionField lastCancel{};

  int insert(CThostFtdcInputOrderField& request, int) override {
    ++inserts;
    lastInsert = request;
    return insertResult;
  }

  int cancel(CThostFtdcInputOrderActionField& request, int) override {
    ++cancels;
    lastCancel = request;
    return cancelResult;
  }

  std::string lastError() const override { return error; }
};

struct Recorder final : ExecutionSink {
  std::vector<OrderUpdate> updates;
  std::vector<Trade> trades;

  void onOrderUpdate(const OrderUpdate& update) override { updates.push_back(update); }
  void onTrade(const Trade& trade) override { trades.push_back(trade); }

  const OrderUpdate& last() const { return updates.back(); }
};

struct Fixture {
  CtpConfig config;
  CtpTraderSession session;
  FakeSender sender;
  Recorder recorder;
  CtpAdapter adapter;

  Fixture() : config(makeConfig()), session(config), adapter(session, sender) {
    adapter.setSink(&recorder);
  }
};

OrderRequest baseRequest(std::uint64_t orderRef) {
  OrderRequest request;
  request.orderRef = orderRef;
  request.instrumentId = "au2612";
  request.side = Side::Buy;
  request.offsetFlag = Offset::Open;
  request.orderType = OrderType::Limit;
  request.timeInForce = TimeInForce::Day;
  request.price = 500.0;
  request.qty = 2;
  request.requestId = "req-" + std::to_string(orderRef);
  return request;
}

CancelRequest cancelFor(std::uint64_t orderRef) {
  CancelRequest request;
  request.orderRef = orderRef;
  request.instrumentId = "au2612";
  request.exchange = "SHFE";
  request.requestId = "cancel-" + std::to_string(orderRef);
  return request;
}

CThostFtdcOrderField orderRow(const std::string& ctpRef, int frontId, int sessionId,
                              char status, int traded, int total) {
  auto row = zeroed<CThostFtdcOrderField>();
  setText(row.OrderRef, ctpRef);
  setText(row.OrderSysID, "sys-" + ctpRef);
  setText(row.InstrumentID, "au2612");
  setText(row.ExchangeID, "SHFE");
  setText(row.TradingDay, "20261124");
  row.Direction = THOST_FTDC_D_Buy;
  row.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
  row.OrderStatus = status;
  row.VolumeTraded = traded;
  row.VolumeTotal = total - traded;
  row.VolumeTotalOriginal = total;
  row.LimitPrice = 500.0;
  row.FrontID = frontId;
  row.SessionID = sessionId;
  return row;
}

CThostFtdcRspInfoField rspError(int errorId, const std::string& message) {
  auto info = zeroed<CThostFtdcRspInfoField>();
  info.ErrorID = errorId;
  setText(info.ErrorMsg, message);
  return info;
}

void checkInsertAccepted() {
  Fixture fixture;
  const OrderRequest request = baseRequest(7);
  check(fixture.adapter.onOrderRequest(request), "a valid order is accepted");

  check(fixture.sender.inserts == 1, "one ReqOrderInsert is sent");
  check(fieldText(fixture.sender.lastInsert.OrderRef) == "7",
        "the contract orderRef becomes the CTP OrderRef");
  check(fieldText(fixture.sender.lastInsert.InstrumentID) == "au2612",
        "the instrument reaches CTP");
  check(fixture.sender.lastInsert.Direction == THOST_FTDC_D_Buy, "side maps to buy");
  check(fixture.sender.lastInsert.CombOffsetFlag[0] == THOST_FTDC_OF_Open,
        "offset maps to open");
  check(fixture.sender.lastInsert.OrderPriceType == THOST_FTDC_OPT_LimitPrice,
        "a limit order uses the limit price type");
  check(fixture.sender.lastInsert.TimeCondition == THOST_FTDC_TC_GFD,
        "DAY maps to good-for-day");
  check(fixture.sender.lastInsert.VolumeCondition == THOST_FTDC_VC_AV,
        "DAY accepts any volume");
  check(fixture.sender.lastInsert.MinVolume == 1, "DAY sets minimum volume 1");
  checkNear(fixture.sender.lastInsert.LimitPrice, 500.0, "price reaches CTP");
  check(fixture.sender.lastInsert.VolumeTotalOriginal == 2, "quantity reaches CTP");
  check(fieldText(fixture.sender.lastInsert.InvestorID) == "900001",
        "the session account reaches CTP");

  check(fixture.recorder.updates.size() == 1, "acceptance publishes one update");
  const OrderUpdate& update = fixture.recorder.last();
  check(update.orderRef == 7, "the update carries the contract orderRef");
  check(update.status == OrderStatus::New, "an accepted order is NEW");
  check(update.orderQty == 2 && update.remainingQty == 2, "quantities are reported");
  checkNear(update.price, 500.0, "the update carries the price");
  check(update.sequence == 1, "the first execution event is sequence 1");
  check(update.accountId == "900001", "the update carries the venue account");
}

void checkTimeInForceMapping() {
  Fixture fixture;
  OrderRequest ioc = baseRequest(8);
  ioc.timeInForce = TimeInForce::Ioc;
  ioc.side = Side::Sell;
  ioc.offsetFlag = Offset::CloseToday;
  check(fixture.adapter.onOrderRequest(ioc), "an IOC close-today order is accepted");
  check(fixture.sender.lastInsert.Direction == THOST_FTDC_D_Sell, "sell maps to sell");
  check(fixture.sender.lastInsert.CombOffsetFlag[0] == THOST_FTDC_OF_CloseToday,
        "close today maps to its own CTP flag");
  check(fixture.sender.lastInsert.TimeCondition == THOST_FTDC_TC_IOC, "IOC maps to CTP IOC");
  check(fixture.sender.lastInsert.VolumeCondition == THOST_FTDC_VC_CV,
        "IOC requires the full volume");
  check(fixture.sender.lastInsert.MinVolume == 2, "IOC sets minimum volume");

  OrderRequest fak = baseRequest(11);
  fak.timeInForce = TimeInForce::Fak;
  check(fixture.adapter.onOrderRequest(fak), "a FAK order is accepted");
  check(fixture.sender.lastInsert.TimeCondition == THOST_FTDC_TC_IOC, "FAK maps to CTP IOC");
  check(fixture.sender.lastInsert.VolumeCondition == THOST_FTDC_VC_AV,
        "FAK accepts any volume");
  check(fixture.sender.lastInsert.MinVolume == 1, "FAK sets minimum volume 1");

  OrderRequest fok = baseRequest(9);
  fok.timeInForce = TimeInForce::Fok;
  check(fixture.adapter.onOrderRequest(fok), "a FOK order is accepted");
  check(fixture.sender.lastInsert.VolumeCondition == THOST_FTDC_VC_CV,
        "FOK requires the full volume");
  check(fixture.sender.lastInsert.MinVolume == 2, "FOK sets the minimum volume");

  OrderRequest market = baseRequest(10);
  market.orderType = OrderType::Market;
  market.price = 0.0;
  check(fixture.adapter.onOrderRequest(market), "a market order needs no price");
  check(fixture.sender.lastInsert.OrderPriceType == THOST_FTDC_OPT_LimitPrice,
        "market DAY keeps the company ORS limit-price type");
  checkNear(fixture.sender.lastInsert.LimitPrice, 0.0, "market DAY sends price 0");
  check(fixture.sender.lastInsert.TimeCondition == THOST_FTDC_TC_GFD,
        "market DAY is good-for-day");
  check(fixture.sender.lastInsert.MinVolume == 1, "market DAY sets minimum volume 1");
}

void checkOffsetMapping() {
  Fixture fixture;
  struct Case {
    Offset offset;
    char ctp;
    const char* name;
  };
  const Case cases[] = {
      {Offset::Open, THOST_FTDC_OF_Open, "open"},
      {Offset::Close, THOST_FTDC_OF_Close, "close"},
      {Offset::CloseToday, THOST_FTDC_OF_CloseToday, "close-today"},
      {Offset::CloseYesterday, THOST_FTDC_OF_Close, "close-yesterday"},
  };
  std::uint64_t orderRef = 20;
  for (const Case& item : cases) {
    OrderRequest request = baseRequest(orderRef++);
    request.offsetFlag = item.offset;
    check(fixture.adapter.onOrderRequest(request),
          std::string(item.name) + " order is accepted");
    check(fixture.sender.lastInsert.CombOffsetFlag[0] == item.ctp,
          std::string(item.name) + " maps 1:1 to CTP");
  }
}

void checkAckOnlyOnce() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);
  const std::size_t afterFirstAck = fixture.recorder.updates.size();
  fixture.adapter.OnRtnOrder(&ack);
  check(fixture.recorder.updates.size() == afterFirstAck,
        "a repeated ACK is not published twice");
}

void checkCancelAccepted() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");

  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);
  check(fixture.recorder.last().status == OrderStatus::Ack, "the venue ACK is reported");
  check(fixture.recorder.last().exchangeOrderId == "sys-7", "the venue order id is carried");
  const std::size_t updatesBeforeCancel = fixture.recorder.updates.size();

  check(fixture.adapter.onCancelRequest(cancelFor(7)), "the cancel is accepted");
  check(fixture.sender.cancels == 1, "one ReqOrderAction is sent");
  check(fixture.sender.lastCancel.ActionFlag == THOST_FTDC_AF_Delete, "the action is a delete");
  check(fixture.sender.lastCancel.FrontID == 3 && fixture.sender.lastCancel.SessionID == 400,
        "the cancel addresses the accepting front/session");
  check(fieldText(fixture.sender.lastCancel.OrderRef) == "7", "the cancel uses the CTP OrderRef");
  check(fieldText(fixture.sender.lastCancel.InstrumentID) == "au2612",
        "the cancel carries the instrument");
  check(fixture.recorder.updates.size() == updatesBeforeCancel,
        "a sent cancel does not publish an intermediate update");
}

void checkCancelReport() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);
  check(fixture.adapter.onCancelRequest(cancelFor(7)), "the cancel is accepted");

  auto canceled = orderRow("7", 3, 400, THOST_FTDC_OST_Canceled, 0, 2);
  fixture.adapter.OnRtnOrder(&canceled);
  check(fixture.recorder.last().status == OrderStatus::Canceled, "the cancel result is reported");
  check(fixture.recorder.last().cancelState == CancelState::Requested,
        "the report keeps the cancel state");
}

void checkCancelRejectedOnOrderReturn() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);
  check(fixture.adapter.onCancelRequest(cancelFor(7)), "the cancel is accepted");
  const std::size_t before = fixture.recorder.updates.size();

  auto rejected = orderRow("7", 3, 400, THOST_FTDC_OST_Canceled, 0, 2);
  rejected.OrderSubmitStatus = THOST_FTDC_OSS_CancelRejected;
  fixture.adapter.OnRtnOrder(&rejected);

  check(fixture.recorder.updates.size() == before + 1,
        "a CancelRejected venue row publishes a result");
  check(fixture.recorder.last().cancelState == CancelState::Rejected,
        "CancelRejected is reported even when the row says Canceled");
  check(fixture.recorder.last().status == OrderStatus::Ack,
        "a rejected cancel leaves the order working, not terminal");
  check(fixture.adapter.onCancelRequest(cancelFor(7)),
        "the order can be cancelled again after the rejection");
}

void checkCancelRejected() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);
  check(fixture.adapter.onCancelRequest(cancelFor(7)), "the cancel is accepted");

  auto action = zeroed<CThostFtdcOrderActionField>();
  setText(action.OrderRef, "7");
  action.FrontID = 3;
  action.SessionID = 400;
  auto info = rspError(25, "cancel rejected by exchange");
  fixture.adapter.OnErrRtnOrderAction(&action, &info);

  check(fixture.recorder.last().cancelState == CancelState::Rejected,
        "a rejected cancel is reported");
  check(fixture.recorder.last().errorCode == 25, "the CTP error id is carried");
  check(fixture.recorder.last().reason == "cancel rejected by exchange",
        "the CTP reason is carried");
  check(fixture.recorder.last().status == OrderStatus::Ack,
        "the order keeps its last known status");
}

void checkRejectCancelRequest() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);

  check(!fixture.adapter.rejectCancelRequest(cancelFor(7), "reconnect pending"),
        "a local cancel rejection is returned");
  check(fixture.recorder.last().cancelState == CancelState::Rejected,
        "the cancel rejection reaches the sink");
  check(fixture.recorder.last().reason == "reconnect pending",
        "the cancel rejection reason is carried");
  check(fixture.sender.cancels == 0, "no ReqOrderAction is sent");
}

void checkCancelBeforeAck() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  check(!fixture.adapter.onCancelRequest(cancelFor(7)),
        "a cancel without a front/session is refused locally");
  check(fixture.sender.cancels == 0, "no doomed ReqOrderAction is sent");
  check(fixture.adapter.lastError().find("front/session") != std::string::npos,
        "the reason names the missing identity");
  check(fixture.recorder.last().cancelState == CancelState::Rejected,
        "the caller still gets a cancel rejection");
}

void checkInsertRejectedByCtp() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted locally");

  auto echo = zeroed<CThostFtdcInputOrderField>();
  setText(echo.OrderRef, "7");
  setText(echo.InstrumentID, "au2612");
  setText(echo.ExchangeID, "SHFE");
  echo.Direction = THOST_FTDC_D_Buy;
  echo.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
  echo.VolumeTotalOriginal = 2;
  echo.LimitPrice = 500.0;
  auto info = rspError(31, "no trading session");
  fixture.adapter.OnRspOrderInsert(&echo, &info);

  const OrderUpdate& update = fixture.recorder.last();
  check(update.status == OrderStatus::Rejected, "a CTP insert error is REJECTED");
  check(update.errorCode == 31, "the CTP error id is carried");
  check(update.reason == "no trading session", "the CTP reason is carried");
  check(update.orderRef == 7, "the rejection is tied to the contract orderRef");
  check(update.remainingQty == 0, "a rejected order has no remaining quantity");
}

void checkInsertRejectedWithZeroError() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted locally");

  auto echo = zeroed<CThostFtdcInputOrderField>();
  setText(echo.OrderRef, "7");
  setText(echo.InstrumentID, "au2612");
  setText(echo.ExchangeID, "SHFE");
  echo.Direction = THOST_FTDC_D_Buy;
  echo.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
  echo.VolumeTotalOriginal = 2;
  echo.LimitPrice = 500.0;
  auto info = rspError(0, "");
  fixture.adapter.OnRspOrderInsert(&echo, &info);

  const OrderUpdate& update = fixture.recorder.last();
  check(update.status == OrderStatus::Rejected,
        "an insert response with error id 0 is still REJECTED");
  check(update.errorCode == 0, "the zero error id is carried");
  check(update.orderRef == 7, "the rejection is tied to the contract orderRef");
  check(update.remainingQty == 0, "a rejected order has no remaining quantity");
}

void checkExchangeRejectInsert() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted locally");

  auto echo = zeroed<CThostFtdcInputOrderField>();
  setText(echo.OrderRef, "7");
  echo.VolumeTotalOriginal = 2;
  echo.LimitPrice = 500.0;
  auto info = rspError(47, "exchange refused");
  fixture.adapter.OnErrRtnOrderInsert(&echo, &info);

  check(fixture.recorder.last().status == OrderStatus::Rejected,
        "an exchange rejection is REJECTED");
  check(fixture.recorder.last().errorCode == 47, "the exchange error id is carried");
}

void checkDuplicateAndRange() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the first order is accepted");
  check(!fixture.adapter.onOrderRequest(baseRequest(7)), "a duplicate orderRef is rejected");
  check(fixture.sender.inserts == 1, "a duplicate sends nothing");
  check(fixture.adapter.lastError().find("already in use") != std::string::npos,
        "the duplicate reason is explicit");
  check(fixture.recorder.last().status == OrderStatus::Rejected,
        "the duplicate is reported as REJECTED");

  check(!fixture.adapter.onOrderRequest(baseRequest(0)), "orderRef 0 is rejected");
  check(!fixture.adapter.onOrderRequest(baseRequest(1000000000000ULL)),
        "a thirteen-digit orderRef is rejected");
  check(fixture.sender.inserts == 1, "an invalid orderRef sends nothing");

  OrderRequest noQuantity = baseRequest(11);
  noQuantity.qty = 0;
  check(!fixture.adapter.onOrderRequest(noQuantity), "a zero quantity is rejected");
  OrderRequest noPrice = baseRequest(12);
  noPrice.price = 0.0;
  check(!fixture.adapter.onOrderRequest(noPrice), "a limit order without price is rejected");
}

void checkSenderFailure() {
  Fixture fixture;
  fixture.sender.insertResult = -5;
  fixture.sender.error = "order requires READY, session is DISCONNECTED";
  check(!fixture.adapter.onOrderRequest(baseRequest(7)), "a failed send is not accepted");
  check(fixture.recorder.last().status == OrderStatus::Rejected,
        "a failed send is reported as REJECTED");
  check(fixture.recorder.last().reason.find("READY") != std::string::npos,
        "the transport reason is carried");
  check(fixture.recorder.last().errorCode == 0, "a local rejection has no CTP error id");
}

void checkUnknownReportsAreBuffered() {
  Fixture fixture;
  auto stranger = orderRow("999", 1, 2, THOST_FTDC_OST_NoTradeQueueing, 0, 1);
  fixture.adapter.OnRtnOrder(&stranger);
  check(fixture.recorder.updates.empty(), "an unknown report is not published");
  check(fixture.adapter.pendingUpdates().size() == 1, "an unknown report is buffered");
  check(fixture.adapter.pendingUpdates().front().orderRef == 0,
        "a buffered report has no contract orderRef");
  check(fixture.adapter.pendingUpdates().front().status == OrderStatus::Ack,
        "the buffered report keeps the venue status");

  auto tradeRow = zeroed<CThostFtdcTradeField>();
  setText(tradeRow.OrderRef, "999");
  setText(tradeRow.TradeID, "T-999");
  setText(tradeRow.InstrumentID, "au2612");
  setText(tradeRow.ExchangeID, "SHFE");
  tradeRow.Direction = THOST_FTDC_D_Buy;
  tradeRow.OffsetFlag = THOST_FTDC_OF_Open;
  tradeRow.Price = 500.0;
  tradeRow.Volume = 1;
  fixture.adapter.OnRtnTrade(&tradeRow);
  check(fixture.recorder.trades.empty(), "an unknown trade is not published");
  check(fixture.adapter.pendingTrades().size() == 1, "an unknown trade is buffered");

  check(!fixture.adapter.onCancelRequest(cancelFor(555)),
        "a cancel for an order this adapter never sent is refused");
  check(fixture.adapter.pendingUpdates().size() == 2,
        "the unknown cancel joins the reconciliation buffer");
  check(fixture.sender.cancels == 0, "an unknown cancel sends nothing");
}

void checkModifyRejected() {
  Fixture fixture;
  ModifyRequest request;
  request.orderRef = 7;
  request.instrumentId = "au2612";
  request.exchange = "SHFE";
  request.price = 505.0;
  request.qty = 2;

  check(!fixture.adapter.onModifyRequest(request), "modify is rejected");
  check(fixture.adapter.lastError() == kModifyUnsupportedReason,
        "the rejection reason is the documented one");
  check(fixture.recorder.last().status == OrderStatus::Rejected, "modify reports REJECTED");
  check(fixture.recorder.last().reason == "modify unsupported by CTP",
        "the wire reason is 'modify unsupported by CTP'");
  check(fixture.recorder.last().errorCode == 0, "modify uses no fabricated error code");
}

void checkTradeExchangeFallback() {
  Fixture fixture;
  OrderRequest request = baseRequest(7);
  request.exchange = "SHFE";
  check(fixture.adapter.onOrderRequest(request), "the order is accepted");

  auto tradeRow = zeroed<CThostFtdcTradeField>();
  setText(tradeRow.OrderRef, "7");
  setText(tradeRow.TradeID, "T-exchange");
  setText(tradeRow.InstrumentID, "au2612");
  // SimNow can leave ExchangeID blank on the trade callback.
  tradeRow.Direction = THOST_FTDC_D_Buy;
  tradeRow.OffsetFlag = THOST_FTDC_OF_Open;
  tradeRow.Price = 500.0;
  tradeRow.Volume = 1;
  fixture.adapter.OnRtnTrade(&tradeRow);

  check(fixture.recorder.trades.size() == 1, "the trade is published");
  check(fixture.recorder.trades.front().exchange == "SHFE",
        "the trade exchange falls back to the order state");

  setText(tradeRow.ExchangeID, "SHFE");
  fixture.adapter.OnRtnTrade(&tradeRow);
  check(fixture.recorder.trades.size() == 1,
        "the same tradeId is deduplicated even when exchange arrives later");
}

void checkDuplicateTradeIgnored() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);

  auto tradeRow = zeroed<CThostFtdcTradeField>();
  setText(tradeRow.OrderRef, "7");
  setText(tradeRow.TradeID, "T1");
  setText(tradeRow.InstrumentID, "au2612");
  setText(tradeRow.ExchangeID, "SHFE");
  setText(tradeRow.TradingDay, "20261124");
  tradeRow.Direction = THOST_FTDC_D_Buy;
  tradeRow.OffsetFlag = THOST_FTDC_OF_Open;
  tradeRow.Price = 501.0;
  tradeRow.Volume = 1;

  fixture.adapter.OnRtnTrade(&tradeRow);
  const std::size_t tradesAfterFirst = fixture.recorder.trades.size();
  const std::size_t updatesAfterFirst = fixture.recorder.updates.size();
  fixture.adapter.OnRtnTrade(&tradeRow);

  check(fixture.recorder.trades.size() == tradesAfterFirst,
        "a duplicate tradeId is not published twice");
  check(fixture.recorder.updates.size() == updatesAfterFirst,
        "a duplicate trade does not republish the order");

  auto partial = orderRow("7", 3, 400, THOST_FTDC_OST_PartTradedQueueing, 1, 2);
  fixture.adapter.OnRtnOrder(&partial);
  check(fixture.recorder.last().filledQty == 1,
        "a duplicate trade is not added to the filled quantity");
}

void checkEmptyTradeIdIgnored() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);

  auto tradeRow = zeroed<CThostFtdcTradeField>();
  setText(tradeRow.OrderRef, "7");
  setText(tradeRow.InstrumentID, "au2612");
  tradeRow.Direction = THOST_FTDC_D_Buy;
  tradeRow.OffsetFlag = THOST_FTDC_OF_Open;
  tradeRow.Price = 500.0;
  tradeRow.Volume = 1;
  fixture.adapter.OnRtnTrade(&tradeRow);

  check(fixture.recorder.trades.empty(), "a trade with no tradeId is not published");
  check(fixture.adapter.lastError().find("tradeId") != std::string::npos,
        "the missing tradeId is reported");
}

void checkTradingDayResetsOrderState() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the first order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);
  auto canceled = orderRow("7", 3, 400, THOST_FTDC_OST_Canceled, 0, 2);
  fixture.adapter.OnRtnOrder(&canceled);

  fixture.adapter.resetTradingDay("20261125");
  check(fixture.adapter.onOrderRequest(baseRequest(7)),
        "an old-day orderRef can be reused after the trading day changes");
  check(fixture.sender.inserts == 2, "the new-day order reaches CTP");
}

void checkTradeAndAveragePrice() {
  Fixture fixture;
  check(fixture.adapter.onOrderRequest(baseRequest(7)), "the order is accepted");
  auto ack = orderRow("7", 3, 400, THOST_FTDC_OST_NoTradeQueueing, 0, 2);
  fixture.adapter.OnRtnOrder(&ack);

  auto tradeRow = zeroed<CThostFtdcTradeField>();
  setText(tradeRow.OrderRef, "7");
  setText(tradeRow.OrderSysID, "sys-7");
  setText(tradeRow.TradeID, "T1");
  setText(tradeRow.InstrumentID, "au2612");
  setText(tradeRow.ExchangeID, "SHFE");
  setText(tradeRow.TradingDay, "20261124");
  tradeRow.Direction = THOST_FTDC_D_Buy;
  tradeRow.OffsetFlag = THOST_FTDC_OF_Open;
  tradeRow.Price = 501.0;
  tradeRow.Volume = 1;
  fixture.adapter.OnRtnTrade(&tradeRow);

  check(fixture.recorder.trades.size() == 1, "a known trade is published");
  const Trade& report = fixture.recorder.trades.front();
  check(report.orderRef == 7, "the trade carries the contract orderRef");
  check(report.tradeId == "T1", "the trade id is carried");
  check(report.qty == 1, "the trade quantity is carried");
  checkNear(report.price, 501.0, "the trade price is carried");
  check(report.side == Side::Buy && report.offsetFlag == Offset::Open,
        "the trade side and offset are mapped");
  check(report.sequence > 0, "the trade gets an execution sequence");

  auto partial = orderRow("7", 3, 400, THOST_FTDC_OST_PartTradedQueueing, 1, 2);
  fixture.adapter.OnRtnOrder(&partial);
  check(fixture.recorder.last().status == OrderStatus::Partial, "a partial fill is PARTIAL");
  checkNear(fixture.recorder.last().avgPrice, 501.0, "avgPrice comes from the trade");
  check(fixture.recorder.last().filledQty == 1, "the filled quantity is carried");
  check(fixture.recorder.last().remainingQty == 1, "the remaining quantity is carried");

  auto allTraded = orderRow("7", 3, 400, THOST_FTDC_OST_AllTraded, 2, 2);
  fixture.adapter.OnRtnOrder(&allTraded);
  check(fixture.recorder.last().status != OrderStatus::Filled,
        "AllTraded alone does not publish FILLED before the trade");

  auto finalTrade = tradeRow;
  setText(finalTrade.TradeID, "T2");
  finalTrade.Price = 502.0;
  finalTrade.Volume = 1;
  fixture.adapter.OnRtnTrade(&finalTrade);

  check(fixture.recorder.last().status == OrderStatus::Filled, "a full fill is FILLED");
  check(fixture.recorder.last().filledQty == 2, "the final fill is accumulated");
  check(fixture.recorder.last().remainingQty == 0, "a filled order has no remaining quantity");
  checkNear(fixture.recorder.last().avgPrice, 501.5, "avgPrice uses all fills");
  check(!fixture.adapter.onCancelRequest(cancelFor(7)), "a terminal order is not cancelled");
  check(fixture.sender.cancels == 0, "no cancel is sent for a filled order");
}

}  // namespace

int main() {
  checkInsertAccepted();
  checkTimeInForceMapping();
  checkOffsetMapping();
  checkAckOnlyOnce();
  checkCancelAccepted();
  checkCancelReport();
  checkCancelRejected();
  checkCancelRejectedOnOrderReturn();
  checkRejectCancelRequest();
  checkCancelBeforeAck();
  checkInsertRejectedByCtp();
  checkInsertRejectedWithZeroError();
  checkExchangeRejectInsert();
  checkDuplicateAndRange();
  checkSenderFailure();
  checkUnknownReportsAreBuffered();
  checkModifyRejected();
  checkTradeExchangeFallback();
  checkDuplicateTradeIgnored();
  checkEmptyTradeIdIgnored();
  checkTradingDayResetsOrderState();
  checkTradeAndAveragePrice();

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_adapter: insert, offset, cancel, reports, modify rejection\n";
  return 0;
}

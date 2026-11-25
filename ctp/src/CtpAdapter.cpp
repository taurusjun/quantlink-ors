#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/PriceSanitize.h"
#include "ors/ctp/TextEncoding.h"

#include <ThostFtdcTraderApi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace ors::ctp {
namespace {

template <std::size_t N>
void copyField(char (&field)[N], const std::string& value) {
  std::snprintf(field, N, "%s", value.c_str());
}

/// CTP char fields are fixed-size arrays that are not always NUL terminated.
template <std::size_t N>
std::string text(const char (&field)[N]) {
  std::size_t length = 0;
  while (length < N && field[length] != '\0') {
    ++length;
  }
  return std::string(field, length);
}

std::uint64_t epochNanos() {
  const auto elapsed = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

OrderStatus mapOrderStatus(char status) {
  switch (status) {
    case THOST_FTDC_OST_AllTraded:
      return OrderStatus::Filled;
    case THOST_FTDC_OST_PartTradedQueueing:
      return OrderStatus::Partial;
    case THOST_FTDC_OST_PartTradedNotQueueing:
      // Part traded, remainder pulled: the order is done, it is not working.
      return OrderStatus::Canceled;
    case THOST_FTDC_OST_NoTradeQueueing:
    case THOST_FTDC_OST_NoTradeNotQueueing:
      return OrderStatus::Ack;
    case THOST_FTDC_OST_Canceled:
      return OrderStatus::Canceled;
    default:
      // 'a' unknown, 'b'/'c' not yet accepted by the exchange.
      return OrderStatus::New;
  }
}

char ctpDirection(Side side) { return side == Side::Sell ? THOST_FTDC_D_Sell : THOST_FTDC_D_Buy; }

Side mapSide(char direction) { return direction == THOST_FTDC_D_Sell ? Side::Sell : Side::Buy; }

char ctpOffset(Offset offset) {
  switch (offset) {
    case Offset::Close:
      return THOST_FTDC_OF_Close;
    case Offset::CloseToday:
      return THOST_FTDC_OF_CloseToday;
    case Offset::CloseYesterday:
      return THOST_FTDC_OF_Close;
    case Offset::Open:
    default:
      return THOST_FTDC_OF_Open;
  }
}

Offset mapOffset(char flag) {
  switch (flag) {
    case THOST_FTDC_OF_Close:
      return Offset::Close;
    case THOST_FTDC_OF_CloseToday:
      return Offset::CloseToday;
    case THOST_FTDC_OF_CloseYesterday:
      return Offset::CloseYesterday;
    case THOST_FTDC_OF_Open:
      return Offset::Open;
    default:
      // shortcut: force-close variants ('2'/'5'/'6') count as CLOSE, SimNow
      // futures never send them for a client order.
      return Offset::Close;
  }
}

/// Sends through the one TraderApi of the session, and only once it is READY.
class SessionSender final : public OrderSender {
 public:
  explicit SessionSender(CtpTraderSession& session) : session_(session) {}

  int insert(CThostFtdcInputOrderField& request, int requestId) override {
    CThostFtdcTraderApi* api = readyApi();
    if (api == nullptr) {
      return -1;
    }
    const int result = api->ReqOrderInsert(&request, requestId);
    if (result != 0) {
      lastError_ = "order insert rejected by CTP rc=" + std::to_string(result);
    }
    return result;
  }

  int cancel(CThostFtdcInputOrderActionField& request, int requestId) override {
    CThostFtdcTraderApi* api = readyApi();
    if (api == nullptr) {
      return -1;
    }
    const int result = api->ReqOrderAction(&request, requestId);
    if (result != 0) {
      lastError_ = "order cancel rejected by CTP rc=" + std::to_string(result);
    }
    return result;
  }

  std::string lastError() const override { return lastError_; }

 private:
  CThostFtdcTraderApi* readyApi() {
    const SessionState state = session_.state();
    if (state != SessionState::Ready) {
      lastError_ = std::string("order requires READY, session is ") + toString(state);
      return nullptr;
    }
    CThostFtdcTraderApi* api = session_.traderApi();
    if (api == nullptr) {
      lastError_ = "order: trader api is not initialized";
    }
    return api;
  }

  CtpTraderSession& session_;
  std::string lastError_;
};

}  // namespace

struct CtpAdapter::Impl {
  /// What the adapter remembers per live order: enough to cancel it and to
  /// report an update whose venue row is incomplete.
  struct OrderState {
    std::string ctpRef;
    std::string instrumentId;
    std::string exchange;
    Side side{Side::Buy};
    Offset offsetFlag{Offset::Open};
    int orderQty{0};
    int filledQty{0};
    int tradeFilledQty{0};
    double filledNotional{0.0};
    double price{0.0};
    std::string exchangeOrderId;
    int frontId{0};
    int sessionId{0};
    OrderStatus status{OrderStatus::New};
    bool ackPublished{false};
    bool cancelRequested{false};
    bool terminal{false};
    std::string requestId;
  };

  Impl(CtpTraderSession& session, OrderSender* sender)
      : session_(session),
        ownedSender_(sender == nullptr ? std::make_unique<SessionSender>(session) : nullptr),
        sender_(sender == nullptr ? *ownedSender_ : *sender) {
    spi_ = std::make_unique<Spi>(*this);
    session_.setOrderSpi(spi_.get());
  }

  ~Impl() { session_.setOrderSpi(nullptr); }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  bool onOrderRequest(const OrderRequest& request) {
    resetTradingDay(session_.tradingDay());
    std::string error;
    if (!validate(request, error)) {
      return reject(request, error);
    }
    const std::string ctpRef = registerRef(request.orderRef);
    if (ctpRef.empty()) {
      return reject(request, "orderRef " + std::to_string(request.orderRef) +
                                 " is invalid or already in use");
    }

    CThostFtdcInputOrderField ctp;
    fillInsert(request, ctpRef, ctp);
    if (sender_.insert(ctp, session_.nextRequestId()) != 0) {
      return reject(request, sender_.lastError());
    }

    OrderState state;
    state.ctpRef = ctpRef;
    state.instrumentId = request.instrumentId;
    state.exchange = request.exchange;
    state.side = request.side;
    state.offsetFlag = request.offsetFlag;
    state.orderQty = request.qty;
    state.price = request.price;
    state.requestId = request.requestId;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      orders_[request.orderRef] = state;
    }

    // Accepted by the CTP front; the exchange acknowledgement follows on
    // OnRtnOrder.
    OrderUpdate update = fromRequest(request);
    update.status = OrderStatus::New;
    publish(update);
    return true;
  }

  bool rejectOrderRequest(const OrderRequest& request, const std::string& reason) {
    return reject(request, reason);
  }

  bool rejectCancelRequest(const CancelRequest& request, const std::string& reason) {
    return rejectCancel(request.orderRef, stateOf(request.orderRef), reason);
  }

  bool onCancelRequest(const CancelRequest& request) {
    resetTradingDay(session_.tradingDay());
    OrderState state;
    bool known = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto found = orders_.find(request.orderRef);
      if (found != orders_.end()) {
        state = found->second;
        known = true;
      }
    }
    if (!known) {
      return rejectUnknownCancel(request);
    }
    if (state.status == OrderStatus::Filled || state.status == OrderStatus::Canceled) {
      return rejectCancel(request.orderRef, state, "order is already terminal");
    }
    if (state.frontId == 0 || state.sessionId == 0) {
      return rejectCancel(request.orderRef, state,
                          "order has no front/session yet; retry after the venue acknowledges it");
    }

    CThostFtdcInputOrderActionField ctp;
    fillCancel(request, state, ctp);
    if (sender_.cancel(ctp, session_.nextRequestId()) != 0) {
      return rejectCancel(request.orderRef, state, sender_.lastError());
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto found = orders_.find(request.orderRef);
      if (found != orders_.end()) {
        found->second.cancelRequested = true;
      }
    }
    return true;
  }

  bool onModifyRequest(const ModifyRequest& request) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = kModifyUnsupportedReason;
    }
    OrderUpdate update;
    update.orderRef = request.orderRef;
    update.status = OrderStatus::Rejected;
    update.side = request.side;
    update.offsetFlag = request.offsetFlag;
    update.orderQty = request.qty;
    update.filledQty = request.filledQty;
    update.price = request.price;
    update.exchange = request.exchange;
    update.requestId = request.requestId;
    update.reason = kModifyUnsupportedReason;
    update.tradingDay = session_.tradingDay();
    publish(update);
    return false;
  }

  void setSink(ExecutionSink* sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = sink;
  }

  void resetTradingDay(const std::string& tradingDay) {
    if (tradingDay.empty()) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!trackingTradingDay_.empty() && tradingDay <= trackingTradingDay_) {
      return;
    }
    orders_.clear();
    refs_.clear();
    pendingUpdates_.clear();
    pendingTrades_.clear();
    trackingTradingDay_ = tradingDay;
  }

  std::string lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
  }

  std::uint64_t sequence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sequence_;
  }

  std::vector<OrderUpdate> pendingUpdates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingUpdates_;
  }

  std::vector<Trade> pendingTrades() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingTrades_;
  }

  /// CTP rejected the insert request itself (bad price, no margin, closed
  /// market, ...). The request was logged as NEW by onOrderRequest, so this
  /// turns it into REJECTED with the venue's reason.
  void onInsertRejected(const CThostFtdcInputOrderField* order,
                        const CThostFtdcRspInfoField* info) {
    if (order == nullptr || info == nullptr) {
      return;
    }
    resetTradingDay(session_.tradingDay());
    const std::uint64_t orderRef = resolve(text(order->OrderRef), 0, 0);
    OrderUpdate update;
    update.orderRef = orderRef;
    update.status = OrderStatus::Rejected;
    update.side = mapSide(order->Direction);
    update.offsetFlag = mapOffset(order->CombOffsetFlag[0]);
    update.orderQty = order->VolumeTotalOriginal;
    update.price = sanitizePrice(order->LimitPrice);
    update.exchange = text(order->ExchangeID);
    update.reason = gbkToUtf8(text(info->ErrorMsg));
    update.errorCode = info->ErrorID;
    update.tradingDay = session_.tradingDay();
    if (orderRef == 0) {
      bufferUpdate(update);
      return;
    }
    markRejected(orderRef);
    publish(update);
  }

  /// CTP accepted the cancel request on the wire; the result of the cancel
  /// itself arrives on OnRtnOrder / OnErrRtnOrderAction.
  void onCancelResponse(const CThostFtdcInputOrderActionField* action,
                        const CThostFtdcRspInfoField* info) {
    if (action == nullptr || info == nullptr || info->ErrorID == 0) {
      return;
    }
    resetTradingDay(session_.tradingDay());
    const std::uint64_t orderRef =
        resolve(text(action->OrderRef), action->FrontID, action->SessionID);
    if (orderRef == 0) {
      bufferCancelReject(text(action->ExchangeID), info);
      return;
    }
    OrderState state = stateOf(orderRef);
    rejectCancel(orderRef, state, gbkToUtf8(text(info->ErrorMsg)), info->ErrorID);
  }

  void onErrorOrderAction(const CThostFtdcOrderActionField* action,
                          const CThostFtdcRspInfoField* info) {
    if (action == nullptr || info == nullptr || info->ErrorID == 0) {
      return;
    }
    resetTradingDay(session_.tradingDay());
    const std::uint64_t orderRef =
        resolve(text(action->OrderRef), action->FrontID, action->SessionID);
    if (orderRef == 0) {
      bufferCancelReject(text(action->ExchangeID), info);
      return;
    }
    OrderState state = stateOf(orderRef);
    rejectCancel(orderRef, state, gbkToUtf8(text(info->ErrorMsg)), info->ErrorID);
  }

  void onRtnOrder(const CThostFtdcOrderField* order) {
    if (order == nullptr) {
      return;
    }
    resetTradingDay(session_.tradingDay());
    const std::string ctpRef = text(order->OrderRef);
    const int frontId = order->FrontID;
    const int sessionId = order->SessionID;
    const std::uint64_t orderRef = resolve(ctpRef, frontId, sessionId);
    if (orderRef == 0) {
      OrderUpdate update;
      update.status = mapOrderStatus(order->OrderStatus);
      update.exchangeOrderId = text(order->OrderSysID);
      update.exchange = text(order->ExchangeID);
      update.side = mapSide(order->Direction);
      update.offsetFlag = mapOffset(order->CombOffsetFlag[0]);
      update.orderQty = order->VolumeTotalOriginal;
      update.filledQty = order->VolumeTraded;
      update.remainingQty = order->VolumeTotal;
      update.price = sanitizePrice(order->LimitPrice);
      update.reason = gbkToUtf8(text(order->StatusMsg));
      update.tradingDay = text(order->TradingDay);
      bufferUpdate(update);
      return;
    }
    bindRef(orderRef, frontId, sessionId);

    const std::string sysId = text(order->OrderSysID);
    const std::string instrument = text(order->InstrumentID);
    const std::string exchange = text(order->ExchangeID);
    const std::string tradingDay = text(order->TradingDay);
    const std::string statusMsg = gbkToUtf8(text(order->StatusMsg));
    const int volumeOriginal = order->VolumeTotalOriginal;
    const double limitPrice = sanitizePrice(order->LimitPrice);
    const OrderStatus mapped = mapOrderStatus(order->OrderStatus);

    OrderUpdate update;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      OrderState& state = orders_[orderRef];
      if (state.terminal) {
        return;
      }
      if (state.ctpRef.empty()) {
        state.ctpRef = ctpRef;
      }
      if (!instrument.empty()) {
        state.instrumentId = instrument;
      }
      if (!exchange.empty()) {
        state.exchange = exchange;
      }
      if (!sysId.empty()) {
        state.exchangeOrderId = sysId;
      }
      if (volumeOriginal != 0) {
        state.orderQty = volumeOriginal;
      }
      if (limitPrice > 0.0) {
        state.price = limitPrice;
      }
      if (frontId != 0 && sessionId != 0) {
        state.frontId = frontId;
        state.sessionId = sessionId;
      }
      bool publishUpdate = false;
      if (order->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected) {
        // The cancel failed; the order is still working. Handle this before
        // the Canceled/Filled mapping, which would otherwise make it terminal.
        state.cancelRequested = false;
        state.ackPublished = true;
        state.status = state.tradeFilledQty > 0 ? OrderStatus::Partial : OrderStatus::Ack;
        publishUpdate = true;
      } else if (order->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected) {
        state.status = OrderStatus::Rejected;
        state.terminal = true;
        publishUpdate = true;
      } else if (mapped == OrderStatus::Canceled) {
        state.status = OrderStatus::Canceled;
        state.terminal = true;
        publishUpdate = true;
      } else if (mapped == OrderStatus::Filled) {
        // AllTraded is not the fill event: wait for OnRtnTrade.
        state.status = state.tradeFilledQty >= state.orderQty ? OrderStatus::Filled
                                                              : OrderStatus::Partial;
      } else if (mapped == OrderStatus::Ack || mapped == OrderStatus::Partial) {
        if (!state.ackPublished && state.filledQty == 0) {
          state.ackPublished = true;
          state.status = OrderStatus::Ack;
          publishUpdate = true;
        } else {
          state.ackPublished = true;
          if (state.filledQty > 0) {
            state.status = OrderStatus::Partial;
          }
        }
      }

      if (!publishUpdate) {
        return;
      }

      update = fromState(orderRef, state);
      update.cancelState = order->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected
                               ? CancelState::Rejected
                               : (state.cancelRequested ? CancelState::Requested
                                                        : CancelState::None);
      update.reason = statusMsg;
      if (!tradingDay.empty()) {
        update.tradingDay = tradingDay;
      }
    }
    publish(update);
  }

  // CTP's private trade callback is treated as at-most-once: this adapter has
  // no Aeron archive/replay, and reconnect queries only update order state,
  // never republish Trade. If replay is ever introduced, L2 dedups by the
  // contract key (venueId, accountId, tradingDay, tradeId).
  void onRtnTrade(const CThostFtdcTradeField* trade) {
    if (trade == nullptr) {
      return;
    }
    resetTradingDay(session_.tradingDay());
    const std::uint64_t orderRef = resolve(text(trade->OrderRef), 0, 0);

    Trade report;
    report.orderRef = orderRef;
    report.tradeId = text(trade->TradeID);
    report.exchangeOrderId = text(trade->OrderSysID);
    report.instrumentId = text(trade->InstrumentID);
    report.exchange = text(trade->ExchangeID);
    report.side = mapSide(trade->Direction);
    report.offsetFlag = mapOffset(trade->OffsetFlag);
    report.price = sanitizePrice(trade->Price);
    report.qty = trade->Volume;
    report.tradingDay = text(trade->TradingDay);
    if (report.tradeId.empty()) {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = "tradeId is required";
      return;
    }
    if (orderRef == 0) {
      bufferTrade(report);
      return;
    }
    bool publishUpdate = false;
    bool knownOrder = true;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto found = orders_.find(orderRef);
      if (found == orders_.end()) {
        knownOrder = false;
      } else {
        OrderState& state = found->second;
        if (report.exchange.empty()) {
          report.exchange = state.exchange;
        }
        state.tradeFilledQty += report.qty;
        state.filledQty = std::max(state.filledQty, state.tradeFilledQty);
        state.filledNotional += report.price * report.qty;
        if (report.requestId.empty()) report.requestId = state.requestId;
        if (!state.terminal) {
          if (state.orderQty > 0 && state.tradeFilledQty >= state.orderQty) {
            state.status = OrderStatus::Filled;
            state.terminal = true;
          } else {
            state.status = OrderStatus::Partial;
          }
          publishUpdate = true;
        }
      }
    }
    if (!knownOrder) {
      bufferTrade(report);
      return;
    }

    // Trade first, then the order status; FILLED must not arrive before Trade.
    publishTrade(report);
    if (publishUpdate) {
      const OrderState state = stateOf(orderRef);
      OrderUpdate update = fromState(orderRef, state);
      update.cancelState = state.cancelRequested ? CancelState::Requested : CancelState::None;
      publish(update);
    }
  }

 private:
  bool validate(const OrderRequest& request, std::string& error) const {
    if (request.orderRef == 0 || request.orderRef > kMaxCtpOrderRef) {
      error = "orderRef must be 1..999999999999";
      return false;
    }
    if (request.instrumentId.empty()) {
      error = "instrumentId is required";
      return false;
    }
    if (request.qty <= 0) {
      error = "qty must be positive";
      return false;
    }
    const auto side = static_cast<std::uint8_t>(request.side);
    const auto offset = static_cast<std::uint8_t>(request.offsetFlag);
    const auto type = static_cast<std::uint8_t>(request.orderType);
    const auto tif = static_cast<std::uint8_t>(request.timeInForce);
    if (side < 1 || side > 2) {
      error = "side must be BUY or SELL";
      return false;
    }
    if (offset < 1 || offset > 4) {
      error = "offsetFlag must be OPEN, CLOSE, CLOSE_TODAY or CLOSE_YESTERDAY";
      return false;
    }
    if (type < 1 || type > 2) {
      error = "orderType must be LIMIT or MARKET";
      return false;
    }
    if (tif < 1 || tif > 4) {
      error = "timeInForce must be DAY, FAK, FOK or IOC";
      return false;
    }
    if (request.orderType == OrderType::Limit && request.price <= 0.0) {
      error = "limit order needs a positive price";
      return false;
    }
    return true;
  }

  void fillInsert(const OrderRequest& request, const std::string& ctpRef,
                  CThostFtdcInputOrderField& ctp) const {
    std::memset(&ctp, 0, sizeof(ctp));
    const CtpConfig& config = session_.config();
    copyField(ctp.BrokerID, config.brokerId);
    copyField(ctp.InvestorID, config.investorId);
    copyField(ctp.UserID, config.investorId);
    copyField(ctp.OrderRef, ctpRef);
    copyField(ctp.InstrumentID, request.instrumentId);
    // CTP infers the exchange from the instrument; SimNow rejects a populated
    // InputOrderField.ExchangeID for ordinary orders.
    ctp.Direction = ctpDirection(request.side);
    ctp.CombOffsetFlag[0] = ctpOffset(request.offsetFlag);
    ctp.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
    if (request.orderType == OrderType::Market) {
      ctp.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
      ctp.LimitPrice = 0;
    } else {
      ctp.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
      ctp.LimitPrice = request.price;
    }
    ctp.VolumeTotalOriginal = request.qty;
    switch (request.timeInForce) {
      case TimeInForce::Ioc:
        ctp.TimeCondition = THOST_FTDC_TC_IOC;
        ctp.VolumeCondition = THOST_FTDC_VC_CV;
        ctp.MinVolume = request.qty;
        break;
      case TimeInForce::Fak:
        ctp.TimeCondition = THOST_FTDC_TC_IOC;
        ctp.VolumeCondition = THOST_FTDC_VC_AV;
        ctp.MinVolume = 1;
        break;
      case TimeInForce::Fok:
        ctp.TimeCondition = THOST_FTDC_TC_IOC;
        ctp.VolumeCondition = THOST_FTDC_VC_CV;
        ctp.MinVolume = request.qty;
        break;
      case TimeInForce::Day:
      default:
        ctp.TimeCondition = THOST_FTDC_TC_GFD;
        ctp.VolumeCondition = THOST_FTDC_VC_AV;
        ctp.MinVolume = 1;
        if (request.orderType == OrderType::Market) {
          ctp.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        }
        break;
    }
    ctp.ContingentCondition = THOST_FTDC_CC_Immediately;
    ctp.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
  }

  void fillCancel(const CancelRequest& request, const OrderState& state,
                  CThostFtdcInputOrderActionField& ctp) const {
    std::memset(&ctp, 0, sizeof(ctp));
    const CtpConfig& config = session_.config();
    copyField(ctp.BrokerID, config.brokerId);
    copyField(ctp.InvestorID, config.investorId);
    copyField(ctp.UserID, config.investorId);
    copyField(ctp.OrderRef, state.ctpRef);
    copyField(ctp.InstrumentID,
              request.instrumentId.empty() ? state.instrumentId : request.instrumentId);
    copyField(ctp.ExchangeID, request.exchange.empty() ? state.exchange : request.exchange);
    ctp.FrontID = state.frontId;
    ctp.SessionID = state.sessionId;
    // CTP `ReqOrderAction` supports Delete only; a modify is not attempted here.
    ctp.ActionFlag = THOST_FTDC_AF_Delete;
  }

  OrderUpdate fromRequest(const OrderRequest& request) const {
    OrderUpdate update;
    update.orderRef = request.orderRef;
    update.side = request.side;
    update.offsetFlag = request.offsetFlag;
    update.orderQty = request.qty;
    update.remainingQty = request.qty;
    update.price = request.price;
    update.exchange = request.exchange;
    update.requestId = request.requestId;
    update.tradingDay = session_.tradingDay();
    return update;
  }

  OrderUpdate fromState(std::uint64_t orderRef, const OrderState& state) const {
    OrderUpdate update;
    update.orderRef = orderRef;
    update.status = state.status;
    update.exchangeOrderId = state.exchangeOrderId;
    update.filledQty = state.filledQty;
    update.remainingQty = state.orderQty - state.filledQty;
    if (update.remainingQty < 0) {
      update.remainingQty = 0;
    }
    update.avgPrice =
        state.filledQty > 0 ? state.filledNotional / state.filledQty : 0.0;
    update.side = state.side;
    update.offsetFlag = state.offsetFlag;
    update.orderQty = state.orderQty;
    update.price = state.price;
    update.exchange = state.exchange;
    update.requestId = state.requestId;
    update.tradingDay = session_.tradingDay();
    return update;
  }

  bool reject(const OrderRequest& request, const std::string& reason) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = reason;
    }
    OrderUpdate update = fromRequest(request);
    update.status = OrderStatus::Rejected;
    update.remainingQty = 0;
    update.reason = reason;
    publish(update);
    return false;
  }

  bool rejectCancel(std::uint64_t orderRef, const OrderState& state,
                    const std::string& reason, int errorCode = 0) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = reason;
      const auto found = orders_.find(orderRef);
      if (found != orders_.end()) {
        found->second.cancelRequested = false;
      }
    }
    OrderUpdate update = fromState(orderRef, state);
    update.cancelState = CancelState::Rejected;
    update.reason = reason;
    update.errorCode = errorCode;
    publish(update);
    return false;
  }

  /// A cancel for an order this adapter never sent: no venue identity, so it
  /// joins the reconciliation buffer instead of being reported as an order.
  bool rejectUnknownCancel(const CancelRequest& request) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = "unknown orderRef";
    }
    OrderUpdate update;
    update.orderRef = request.orderRef;
    update.status = OrderStatus::Rejected;
    update.cancelState = CancelState::Rejected;
    update.exchange = request.exchange;
    update.requestId = request.requestId;
    update.reason = "unknown orderRef";
    update.tradingDay = session_.tradingDay();
    bufferUpdate(update);
    return false;
  }

  void markRejected(std::uint64_t orderRef) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = orders_.find(orderRef);
    if (found != orders_.end()) {
      found->second.status = OrderStatus::Rejected;
      found->second.terminal = true;
    }
  }

  OrderState stateOf(std::uint64_t orderRef) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = orders_.find(orderRef);
    return found == orders_.end() ? OrderState{} : found->second;
  }

  std::string registerRef(std::uint64_t orderRef) {
    std::lock_guard<std::mutex> lock(mutex_);
    return refs_.add(orderRef) ? refs_.ctpRef(orderRef) : std::string();
  }

  std::uint64_t resolve(const std::string& ctpRef, int frontId, int sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return refs_.resolve(frontId, sessionId, ctpRef);
  }

  void bindRef(std::uint64_t orderRef, int frontId, int sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    (void)refs_.bind(orderRef, frontId, sessionId);
  }

  void publish(OrderUpdate update) {
    ExecutionSink* sink = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      update.sequence = ++sequence_;
      update.ingestTimestamp = epochNanos();
      update.accountId = session_.config().investorId;
      sink = sink_;
    }
    if (sink != nullptr) {
      sink->onOrderUpdate(update);
    }
  }

  void publishTrade(Trade report) {
    ExecutionSink* sink = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      report.sequence = ++sequence_;
      report.ingestTimestamp = epochNanos();
      report.accountId = session_.config().investorId;
      sink = sink_;
    }
    if (sink != nullptr) {
      sink->onTrade(report);
    }
  }

  void bufferUpdate(OrderUpdate update) {
    std::lock_guard<std::mutex> lock(mutex_);
    update.sequence = ++sequence_;
    update.ingestTimestamp = epochNanos();
    update.accountId = session_.config().investorId;
    pendingUpdates_.push_back(std::move(update));
  }

  /// A cancel rejection whose orderRef this adapter cannot resolve.
  void bufferCancelReject(const std::string& exchange,
                          const CThostFtdcRspInfoField* info) {
    OrderUpdate update;
    update.status = OrderStatus::Rejected;
    update.cancelState = CancelState::Rejected;
    update.exchange = exchange;
    update.reason = gbkToUtf8(text(info->ErrorMsg));
    update.errorCode = info->ErrorID;
    update.tradingDay = session_.tradingDay();
    bufferUpdate(update);
  }

  void bufferTrade(Trade report) {
    std::lock_guard<std::mutex> lock(mutex_);
    report.sequence = ++sequence_;
    report.ingestTimestamp = epochNanos();
    report.accountId = session_.config().investorId;
    pendingTrades_.push_back(std::move(report));
  }

  struct Spi final : CThostFtdcTraderSpi {
    explicit Spi(Impl& owner) : owner_(owner) {}

    void OnRspOrderInsert(CThostFtdcInputOrderField* order, CThostFtdcRspInfoField* info,
                          int, bool) override {
      owner_.onInsertRejected(order, info);
    }

    void OnRspOrderAction(CThostFtdcInputOrderActionField* action,
                          CThostFtdcRspInfoField* info, int, bool) override {
      owner_.onCancelResponse(action, info);
    }

    void OnErrRtnOrderInsert(CThostFtdcInputOrderField* order,
                             CThostFtdcRspInfoField* info) override {
      owner_.onInsertRejected(order, info);
    }

    void OnErrRtnOrderAction(CThostFtdcOrderActionField* action,
                             CThostFtdcRspInfoField* info) override {
      owner_.onErrorOrderAction(action, info);
    }

    void OnRtnOrder(CThostFtdcOrderField* order) override { owner_.onRtnOrder(order); }

    void OnRtnTrade(CThostFtdcTradeField* trade) override { owner_.onRtnTrade(trade); }

    Impl& owner_;
  };

  CtpTraderSession& session_;
  std::unique_ptr<OrderSender> ownedSender_;
  OrderSender& sender_;
  mutable std::mutex mutex_;
  CtpOrderRefMap refs_;
  std::unordered_map<std::uint64_t, OrderState> orders_;
  std::string trackingTradingDay_;
  std::vector<OrderUpdate> pendingUpdates_;
  std::vector<Trade> pendingTrades_;
  std::uint64_t sequence_{0};
  ExecutionSink* sink_{nullptr};
  std::string lastError_;
  std::unique_ptr<Spi> spi_;
};

CtpAdapter::CtpAdapter(CtpTraderSession& session)
    : impl_(std::make_unique<Impl>(session, nullptr)) {}

CtpAdapter::CtpAdapter(CtpTraderSession& session, OrderSender& sender)
    : impl_(std::make_unique<Impl>(session, &sender)) {}

CtpAdapter::~CtpAdapter() = default;

bool CtpAdapter::onOrderRequest(const OrderRequest& request) {
  return impl_->onOrderRequest(request);
}

bool CtpAdapter::onCancelRequest(const CancelRequest& request) {
  return impl_->onCancelRequest(request);
}

bool CtpAdapter::rejectOrderRequest(const OrderRequest& request, const std::string& reason) {
  return impl_->rejectOrderRequest(request, reason);
}

bool CtpAdapter::rejectCancelRequest(const CancelRequest& request, const std::string& reason) {
  return impl_->rejectCancelRequest(request, reason);
}

bool CtpAdapter::onModifyRequest(const ModifyRequest& request) {
  return impl_->onModifyRequest(request);
}

void CtpAdapter::setSink(ExecutionSink* sink) { impl_->setSink(sink); }

void CtpAdapter::resetTradingDay(const std::string& tradingDay) {
  impl_->resetTradingDay(tradingDay);
}

std::string CtpAdapter::lastError() const { return impl_->lastError(); }

std::uint64_t CtpAdapter::sequence() const { return impl_->sequence(); }

std::vector<OrderUpdate> CtpAdapter::pendingUpdates() const {
  return impl_->pendingUpdates();
}

std::vector<Trade> CtpAdapter::pendingTrades() const { return impl_->pendingTrades(); }

void CtpAdapter::OnRspOrderInsert(CThostFtdcInputOrderField* order,
                                  CThostFtdcRspInfoField* info) {
  impl_->onInsertRejected(order, info);
}

void CtpAdapter::OnRspOrderAction(CThostFtdcInputOrderActionField* action,
                                  CThostFtdcRspInfoField* info) {
  impl_->onCancelResponse(action, info);
}

void CtpAdapter::OnErrRtnOrderInsert(CThostFtdcInputOrderField* order,
                                     CThostFtdcRspInfoField* info) {
  impl_->onInsertRejected(order, info);
}

void CtpAdapter::OnErrRtnOrderAction(CThostFtdcOrderActionField* action,
                                     CThostFtdcRspInfoField* info) {
  impl_->onErrorOrderAction(action, info);
}

void CtpAdapter::OnRtnOrder(CThostFtdcOrderField* order) { impl_->onRtnOrder(order); }

void CtpAdapter::OnRtnTrade(CThostFtdcTradeField* trade) { impl_->onRtnTrade(trade); }

const char* toString(OrderStatus status) {
  switch (status) {
    case OrderStatus::New:
      return "NEW";
    case OrderStatus::Ack:
      return "ACK";
    case OrderStatus::Partial:
      return "PARTIAL";
    case OrderStatus::Filled:
      return "FILLED";
    case OrderStatus::Canceled:
      return "CANCELED";
    case OrderStatus::Rejected:
      return "REJECTED";
  }
  return "UNKNOWN";
}

const char* toString(CancelState state) {
  switch (state) {
    case CancelState::None:
      return "NONE";
    case CancelState::Requested:
      return "REQUESTED";
    case CancelState::Rejected:
      return "REJECTED";
  }
  return "UNKNOWN";
}

}  // namespace ors::ctp

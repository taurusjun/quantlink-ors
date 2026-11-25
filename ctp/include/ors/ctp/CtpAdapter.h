#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/CtpOrderRefMap.h"
#include "ors/ctp/CtpSession.h"

// SDK types stay incomplete here; only pointers and references cross this
// header, so the CTP headers remain in the .cpp.
class CThostFtdcTraderSpi;
struct CThostFtdcInputOrderField;
struct CThostFtdcInputOrderActionField;
struct CThostFtdcOrderActionField;
struct CThostFtdcOrderField;
struct CThostFtdcRspInfoField;
struct CThostFtdcTradeField;

namespace ors::ctp {

/// L0 contract values; CtpAdapter maps them to the CTP char codes.
enum class Side : std::uint8_t { Buy = 1, Sell = 2 };
enum class Offset : std::uint8_t { Open = 1, Close = 2, CloseToday = 3, CloseYesterday = 4 };
enum class OrderType : std::uint8_t { Limit = 1, Market = 2 };
enum class TimeInForce : std::uint8_t { Day = 1, Fak = 2, Fok = 3, Ioc = 4 };
enum class OrderStatus : std::uint8_t {
  New = 1,
  Ack = 2,
  Partial = 3,
  Filled = 4,
  Canceled = 5,
  Rejected = 6,
};
enum class CancelState : std::uint8_t { None = 0, Requested = 1, Rejected = 2 };

const char* toString(OrderStatus status);
const char* toString(CancelState state);

struct OrderRequest {
  std::uint64_t orderRef{0};
  std::string instrumentId;
  std::string exchange;
  Side side{Side::Buy};
  Offset offsetFlag{Offset::Open};
  OrderType orderType{OrderType::Limit};
  TimeInForce timeInForce{TimeInForce::Day};
  double price{0.0};
  int qty{0};
  std::string accountId;
  std::string venueId;
  std::string requestId;
  std::uint8_t environment{0};
  std::uint64_t sourceSessionId{0};
  std::uint64_t sourcePublicationEpoch{0};
};

struct CancelRequest {
  std::uint64_t orderRef{0};
  std::string instrumentId;
  std::string exchange;
  std::string accountId;
  std::string requestId;
  std::string venueId;
  std::uint8_t environment{0};
  std::uint64_t sourceSessionId{0};
  std::uint64_t sourcePublicationEpoch{0};
};

/// CTP has no modify: CtpAdapter rejects it with
/// kModifyUnsupportedReason and never cancels+replaces behind the caller.
struct ModifyRequest {
  std::uint64_t orderRef{0};
  std::string instrumentId;
  std::string exchange;
  Side side{Side::Buy};
  Offset offsetFlag{Offset::Open};
  OrderType orderType{OrderType::Limit};
  TimeInForce timeInForce{TimeInForce::Day};
  double price{0.0};
  int qty{0};
  int filledQty{0};
  int disclosedQty{0};
  std::string accountId;
  std::string venueId;
  std::string requestId;
  std::uint8_t environment{0};
  std::uint64_t sourceSessionId{0};
  std::uint64_t sourcePublicationEpoch{0};
};

inline constexpr const char* kModifyUnsupportedReason = "modify unsupported by CTP";

/// One L0 OrderUpdate. venueId/environment/sourceSessionId/epoch default to 0
/// and are filled by whoever owns the contract context (Task 6).
struct OrderUpdate {
  std::uint64_t orderRef{0};
  std::string exchangeOrderId;
  OrderStatus status{OrderStatus::New};
  CancelState cancelState{CancelState::None};
  int filledQty{0};
  int remainingQty{0};
  double avgPrice{0.0};
  std::string reason;
  int errorCode{0};
  std::string tradingDay;
  std::uint64_t exchangeTimestamp{0};  ///< 0: CTP gave no epoch timestamp
  std::uint64_t ingestTimestamp{0};
  std::string requestId;
  std::uint64_t sequence{0};
  Side side{Side::Buy};
  Offset offsetFlag{Offset::Open};
  int orderQty{0};
  double price{0.0};
  std::string accountId;
  std::string venueId;
  std::string exchange;
  std::uint8_t environment{0};
  std::uint64_t sourceSessionId{0};
  std::uint64_t sourcePublicationEpoch{0};
};

/// One L0 Trade; fee is left 0/false for Task 6 to fill.
struct Trade {
  std::string tradeId;
  std::uint64_t orderRef{0};
  std::string exchangeOrderId;
  std::uint64_t sequence{0};
  std::string instrumentId;
  std::string exchange;
  Side side{Side::Buy};
  Offset offsetFlag{Offset::Open};
  double price{0.0};
  int qty{0};
  std::uint64_t exchangeTimestamp{0};
  std::uint64_t ingestTimestamp{0};
  std::string tradingDay;
  std::string requestId;
  std::string accountId;
  std::string venueId;
  std::uint8_t environment{0};
  std::uint64_t sourceSessionId{0};
  std::uint64_t sourcePublicationEpoch{0};
};

/**
 * Receives execution events on the thread CTP called back on (or on the
 * sending thread for the request-accepted update). The sink must outlive the
 * adapter.
 */
class ExecutionSink {
 public:
  virtual ~ExecutionSink() = default;
  virtual void onOrderUpdate(const OrderUpdate& update) = 0;
  virtual void onTrade(const Trade& trade) = 0;
};

/**
 * The two order requests CtpAdapter sends; the production implementation wraps
 * the session's TraderApi, while a test injects its own and stays offline.
 */
class OrderSender {
 public:
  virtual ~OrderSender() = default;
  virtual int insert(CThostFtdcInputOrderField& request, int requestId) = 0;
  virtual int cancel(CThostFtdcInputOrderActionField& request, int requestId) = 0;
  /// Why the last insert/cancel failed; a rejected send sets both rc != 0 and
  /// this message.
  virtual std::string lastError() const = 0;
};

/**
 * Routes L0 order/cancel requests to CTP and normalizes the execution reports.
 *
 * One adapter per TraderApi session: it registers itself as the session's order
 * SPI, so it must be destroyed before the session (or detached, which the
 * destructor does). Requests before READY are rejected with an OrderUpdate
 * instead of being dropped. Reports whose CTP OrderRef is not known are kept in
 * pendingUpdates()/pendingTrades() with orderRef 0 for the reconciler (Task 7)
 * instead of being thrown away.
 *
 * ModifyRequest is rejected: CTP `ReqOrderAction` only supports Delete.
 */
class CtpAdapter {
 public:
  explicit CtpAdapter(CtpTraderSession& session);
  /// Uses a caller-provided transport; the test seam, never production.
  CtpAdapter(CtpTraderSession& session, OrderSender& sender);
  ~CtpAdapter();

  CtpAdapter(const CtpAdapter&) = delete;
  CtpAdapter& operator=(const CtpAdapter&) = delete;

  bool onOrderRequest(const OrderRequest& request);
  bool onCancelRequest(const CancelRequest& request);

  /// Publishes a local REJECTED without sending to CTP. Used by the runtime
  /// while reconnect reconciliation is pending.
  bool rejectOrderRequest(const OrderRequest& request, const std::string& reason);

  /// Publishes a local cancel rejection without sending ReqOrderAction.
  bool rejectCancelRequest(const CancelRequest& request, const std::string& reason);
  /// Always false; emits a REJECTED OrderUpdate naming the reason.
  bool onModifyRequest(const ModifyRequest& request);

  /// May be null; events go nowhere until it is set.
  void setSink(ExecutionSink* sink);
  std::string lastError() const;

  /// Execution stream sequence of this adapter (OrderUpdate and Trade share it).
  std::uint64_t sequence() const;

  /// Reports CTP sent for an OrderRef this adapter never issued, oldest first.
  std::vector<OrderUpdate> pendingUpdates() const;
  std::vector<Trade> pendingTrades() const;

  /// CTP SPI entry points, called by the session's forwarding SPI. Public so
  /// the offline test can feed rows without a session.
  void OnRspOrderInsert(CThostFtdcInputOrderField* order, CThostFtdcRspInfoField* info);
  void OnRspOrderAction(CThostFtdcInputOrderActionField* action,
                        CThostFtdcRspInfoField* info);
  void OnErrRtnOrderInsert(CThostFtdcInputOrderField* order,
                           CThostFtdcRspInfoField* info);
  void OnErrRtnOrderAction(CThostFtdcOrderActionField* action,
                           CThostFtdcRspInfoField* info);
  void OnRtnOrder(CThostFtdcOrderField* order);
  void OnRtnTrade(CThostFtdcTradeField* trade);

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace ors::ctp

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/CtpSession.h"

// The CTP row types stay incomplete here: only the mapping functions below and
// their callers (which include the SDK header) need them.
struct CThostFtdcTradingAccountField;
struct CThostFtdcInvestorPositionField;
struct CThostFtdcOrderField;
struct CThostFtdcTradeField;

namespace ors::ctp {

/// How long a snapshot stays usable; consumers reject an older one.
inline constexpr std::uint32_t kDefaultSnapshotStaleAfterMs = 5000;

/**
 * Batch header of one query.
 *
 * Every row of a single query carries the same values here, so rows are never
 * stamped one by one: a consumer can tell a batch apart by snapshotId alone and
 * must reject the batch once staleAfterMs has elapsed.
 */
struct SnapshotHeader {
  std::string accountId;  ///< CTP InvestorID / ORS venue account
  std::string tradingDay;
  std::string snapshotId;
  std::uint64_t snapshotTimestamp{0};  ///< epoch nanoseconds
  std::uint32_t staleAfterMs{kDefaultSnapshotStaleAfterMs};
};

struct AccountSnapshot {
  SnapshotHeader header;
  double balance{0.0};  ///< CTP Balance: futures dynamic equity, not static
  double available{0.0};
  double margin{0.0};
  double frozenMargin{0.0};
  double commission{0.0};
  double closeProfit{0.0};
  double positionProfit{0.0};
  double deposit{0.0};
  double withdraw{0.0};
};

struct PositionSnapshot {
  SnapshotHeader header;
  std::string instrumentId;
  std::string exchange;
  int longQty{0};
  int shortQty{0};
  int todayLong{0};
  int todayShort{0};
  int yesterdayLong{0};
  int yesterdayShort{0};
  double avgPrice{0.0};  ///< quantity-weighted over both sides
  double longAvgPrice{0.0};
  double shortAvgPrice{0.0};
  double settlementPrice{0.0};
};

struct OrderSnapshot {
  SnapshotHeader header;
  std::string orderRef;  ///< CTP OrderRef; the L0 orderRef mapping is Task 5
  std::string orderSysId;
  std::string instrumentId;
  std::string exchange;
  char direction{0};
  char offsetFlag{0};
  char orderStatus{0};  ///< raw CTP status; the L0 enum mapping is Task 5
  int volumeTraded{0};
  int volumeTotal{0};
  int volumeTotalOriginal{0};
  double limitPrice{0.0};
  // ReqOrderAction addresses an order by (FrontID, SessionID, OrderRef).
  int frontId{0};
  int sessionId{0};
};

struct TradeSnapshot {
  SnapshotHeader header;
  std::string tradeId;
  std::string orderRef;
  std::string orderSysId;
  std::string instrumentId;
  std::string exchange;
  char direction{0};
  char offsetFlag{0};
  int volume{0};
  double price{0.0};
  std::string tradeDate;
  std::string tradeTime;
};

/// Result of the order/trade pair. Rows keep the header of their own query, so
/// an orders-only run leaves trades empty instead of inventing a timestamp.
struct ReconciliationSnapshot {
  std::vector<OrderSnapshot> orders;
  std::vector<TradeSnapshot> trades;
};

/**
 * Receives finished snapshots on the thread that ran the query, never from a
 * CTP callback. A failed query publishes nothing.
 */
class SnapshotSink {
 public:
  virtual ~SnapshotSink() = default;

  virtual void onSnapshot(const AccountSnapshot& snapshot) = 0;
  virtual void onSnapshot(const std::vector<PositionSnapshot>& snapshot) = 0;
  virtual void onSnapshot(const ReconciliationSnapshot& snapshot) = 0;
};

AccountSnapshot mapAccountSnapshot(const CThostFtdcTradingAccountField& row,
                                   const SnapshotHeader& header);

/// One CTP row is one direction; volumeMultiple comes from the instrument
/// table and is only needed to derive the average price.
PositionSnapshot mapPositionSnapshot(const CThostFtdcInvestorPositionField& row,
                                     const SnapshotHeader& header, int volumeMultiple);

OrderSnapshot mapOrderSnapshot(const CThostFtdcOrderField& row,
                               const SnapshotHeader& header);

TradeSnapshot mapTradeSnapshot(const CThostFtdcTradeField& row,
                               const SnapshotHeader& header);

/// Folds the per-direction rows into one row per instrument and drops
/// instruments with no position left on either side.
std::vector<PositionSnapshot> mergePositions(std::vector<PositionSnapshot> rows);

/**
 * Account/position/order/trade queries on an existing TraderApi session.
 *
 * One query runs at a time and blocks until CTP marks its response complete.
 * Queries are only accepted in READY; a rejected, failed or timed-out query
 * reports through lastError() and publishes no snapshot, so an empty snapshot
 * always means the venue really reported nothing.
 *
 * The service must be destroyed before the session it was built on.
 */
class CtpQueryService {
 public:
  explicit CtpQueryService(CtpTraderSession& session);
  ~CtpQueryService();

  CtpQueryService(const CtpQueryService&) = delete;
  CtpQueryService& operator=(const CtpQueryService&) = delete;

  bool queryAccount(std::chrono::milliseconds timeout = std::chrono::seconds(10));
  bool queryPositions(std::chrono::milliseconds timeout = std::chrono::seconds(10));
  bool queryOrders(std::chrono::milliseconds timeout = std::chrono::seconds(10));
  bool queryTrades(std::chrono::milliseconds timeout = std::chrono::seconds(10));

  /// May be null; the sink must outlive the service.
  void setSink(SnapshotSink* sink);
  std::string lastError() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ors::ctp

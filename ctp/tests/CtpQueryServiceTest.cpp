// CtpQueryService tests: batch header aggregation, row mapping (including the
// DBL_MAX/NaN sentinels) and the no-snapshot-on-failure rule. The mapping tests
// build CTP rows directly; the failure test never touches the network.

#include <ThostFtdcTraderApi.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "ors/ctp/CtpQueryService.h"

namespace {

using ors::ctp::AccountSnapshot;
using ors::ctp::CtpConfig;
using ors::ctp::CtpQueryService;
using ors::ctp::CtpTraderSession;
using ors::ctp::mapAccountSnapshot;
using ors::ctp::mapOrderSnapshot;
using ors::ctp::mapPositionSnapshot;
using ors::ctp::mapTradeSnapshot;
using ors::ctp::mergePositions;
using ors::ctp::PositionSnapshot;
using ors::ctp::ReconciliationSnapshot;
using ors::ctp::SnapshotHeader;
using ors::ctp::SnapshotSink;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

void checkNear(double actual, double expected, const std::string& what) {
  check(std::fabs(actual - expected) < 1e-9, what);
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

SnapshotHeader batch() {
  SnapshotHeader header;
  header.accountId = "900001";
  header.tradingDay = "20261124";
  header.snapshotId = "20261124-POS-1";
  header.snapshotTimestamp = 1732435200000ULL;
  return header;
}

CThostFtdcInvestorPositionField positionRow(char direction, int position, int today,
                                           double cost, double settlement) {
  auto row = zeroed<CThostFtdcInvestorPositionField>();
  setText(row.InstrumentID, "rb2601");
  setText(row.ExchangeID, "SHFE");
  row.PosiDirection = direction;
  row.Position = position;
  row.TodayPosition = today;
  row.YdPosition = position - today;
  row.PositionCost = cost;
  row.SettlementPrice = settlement;
  return row;
}

void checkBatchHeader() {
  const SnapshotHeader header = batch();
  const auto longRow = positionRow(THOST_FTDC_PD_Long, 4, 1, 4 * 3400.0 * 10, 3390.0);
  const auto shortRow = positionRow(THOST_FTDC_PD_Short, 2, 0, 2 * 3410.0 * 10, 3390.0);

  std::vector<PositionSnapshot> rows;
  rows.push_back(mapPositionSnapshot(longRow, header, 10));
  rows.push_back(mapPositionSnapshot(shortRow, header, 10));
  const std::vector<PositionSnapshot> merged = mergePositions(rows);

  check(merged.size() == 1, "long and short rows merge into one instrument row");
  if (merged.size() != 1) {
    return;
  }
  const PositionSnapshot& row = merged.front();
  // One snapshotId must mean one tradingDay and one timestamp for every row.
  check(row.header.snapshotId == header.snapshotId, "merged row keeps the batch snapshotId");
  check(row.header.tradingDay == header.tradingDay, "merged row keeps the batch tradingDay");
  check(row.header.snapshotTimestamp == header.snapshotTimestamp,
        "merged row keeps the batch snapshotTimestamp");
  check(row.header.accountId == "900001", "batch carries the venue account");

  check(row.longQty == 4 && row.shortQty == 2, "long and short quantities are kept apart");
  check(row.todayLong == 1 && row.yesterdayLong == 3, "long today/yesterday split");
  check(row.todayShort == 0 && row.yesterdayShort == 2, "short today/yesterday split");
  checkNear(row.longAvgPrice, 3400.0, "long average price from PositionCost");
  checkNear(row.shortAvgPrice, 3410.0, "short average price from PositionCost");
  checkNear(row.avgPrice, (3400.0 * 4 + 3410.0 * 2) / 6, "avgPrice is quantity weighted");
  checkNear(row.settlementPrice, 3390.0, "settlement price copied");
}

void checkPositionSentinels() {
  const SnapshotHeader header = batch();
  auto row = positionRow(THOST_FTDC_PD_Long, 3, 3,
                         std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::quiet_NaN());
  const PositionSnapshot snapshot = mapPositionSnapshot(row, header, 10);
  checkNear(snapshot.longAvgPrice, 0.0, "DBL_MAX PositionCost yields no average price");
  checkNear(snapshot.settlementPrice, 0.0, "NaN settlement price maps to zero");
  check(snapshot.longQty == 3, "sanitising a price leaves the quantity alone");

  // A flat instrument must not survive the merge as a phantom position.
  auto flat = positionRow(THOST_FTDC_PD_Long, 0, 0, 0.0, 3390.0);
  check(mergePositions({mapPositionSnapshot(flat, header, 10)}).empty(),
        "flat rows are dropped");
}

void checkAccountMapping() {
  const SnapshotHeader header = batch();
  auto row = zeroed<CThostFtdcTradingAccountField>();
  row.Balance = 1000000.5;
  row.Available = std::numeric_limits<double>::max();
  row.CurrMargin = 12000.0;
  row.FrozenMargin = 500.0;
  row.Commission = std::numeric_limits<double>::quiet_NaN();
  row.CloseProfit = -300.0;
  row.PositionProfit = 1200.0;
  row.Deposit = 200000.0;
  row.Withdraw = 3000.0;
  setText(row.TradingDay, "20261124");

  const AccountSnapshot snapshot = mapAccountSnapshot(row, header);
  checkNear(snapshot.balance, 1000000.5, "balance copied");
  checkNear(snapshot.available, 0.0, "DBL_MAX available maps to zero");
  checkNear(snapshot.margin, 12000.0, "margin copied from CurrMargin");
  checkNear(snapshot.frozenMargin, 500.0, "frozen margin copied");
  checkNear(snapshot.commission, 0.0, "NaN commission maps to zero");
  checkNear(snapshot.closeProfit, -300.0, "close profit keeps its sign");
  checkNear(snapshot.positionProfit, 1200.0, "position profit copied");
  checkNear(snapshot.deposit, 200000.0, "deposit copied");
  checkNear(snapshot.withdraw, 3000.0, "withdraw copied");
  check(snapshot.header.snapshotId == header.snapshotId, "account keeps the batch header");
}

void checkOrderAndTradeMapping() {
  const SnapshotHeader header = batch();

  auto order = zeroed<CThostFtdcOrderField>();
  setText(order.OrderRef, "42");
  setText(order.OrderSysID, "     1234");
  setText(order.InstrumentID, "rb2601");
  setText(order.ExchangeID, "SHFE");
  order.Direction = THOST_FTDC_D_Buy;
  order.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
  order.OrderStatus = THOST_FTDC_OST_AllTraded;
  order.VolumeTraded = 2;
  order.VolumeTotal = 2;
  order.VolumeTotalOriginal = 3;
  order.LimitPrice = std::numeric_limits<double>::max();
  order.FrontID = 7;
  order.SessionID = 9001;

  const auto orderSnapshot = mapOrderSnapshot(order, header);
  check(orderSnapshot.orderRef == "42", "orderRef copied");
  check(orderSnapshot.instrumentId == "rb2601" && orderSnapshot.exchange == "SHFE",
        "order instrument and exchange copied");
  check(orderSnapshot.direction == THOST_FTDC_D_Buy, "order direction copied");
  check(orderSnapshot.offsetFlag == THOST_FTDC_OF_Open, "order offset flag copied");
  check(orderSnapshot.orderStatus == THOST_FTDC_OST_AllTraded, "order status kept raw");
  check(orderSnapshot.volumeTraded == 2 && orderSnapshot.volumeTotal == 2, "order volumes copied");
  check(orderSnapshot.volumeTotalOriginal == 3, "original volume copied");
  // Cancel needs the same (FrontID, SessionID, OrderRef) triple ReqOrderAction takes.
  check(orderSnapshot.frontId == 7 && orderSnapshot.sessionId == 9001,
        "front and session id copied for ReqOrderAction");
  checkNear(orderSnapshot.limitPrice, 0.0, "DBL_MAX limit price maps to zero");
  check(orderSnapshot.header.snapshotId == header.snapshotId, "order keeps the batch header");

  auto trade = zeroed<CThostFtdcTradeField>();
  setText(trade.TradeID, "T1");
  setText(trade.OrderRef, "42");
  setText(trade.InstrumentID, "rb2601");
  setText(trade.ExchangeID, "SHFE");
  setText(trade.TradeDate, "20261124");
  setText(trade.TradeTime, "10:15:00");
  trade.Direction = THOST_FTDC_D_Sell;
  trade.OffsetFlag = THOST_FTDC_OF_CloseToday;
  trade.Volume = 2;
  trade.Price = 3412.5;

  const auto tradeSnapshot = mapTradeSnapshot(trade, header);
  check(tradeSnapshot.tradeId == "T1", "tradeId copied");
  check(tradeSnapshot.orderRef == "42", "trade orderRef copied");
  checkNear(tradeSnapshot.price, 3412.5, "trade price copied");
  check(tradeSnapshot.volume == 2, "trade volume copied");
  check(tradeSnapshot.tradeDate == "20261124" && tradeSnapshot.tradeTime == "10:15:00",
        "trade date and time copied");
  check(tradeSnapshot.header.snapshotId == header.snapshotId, "trade keeps the batch header");
}

/// Records what the service would publish, so a failure can be proven silent.
struct Recorder final : SnapshotSink {
  int published{0};

  void onSnapshot(const AccountSnapshot&) override { ++published; }
  void onSnapshot(const std::vector<PositionSnapshot>&) override { ++published; }
  void onSnapshot(const ReconciliationSnapshot&) override { ++published; }
};

void checkFailurePublishesNothing() {
  CtpConfig config;
  config.brokerId = "9999";
  config.investorId = "900001";
  config.password = "not-used";
  config.appId = "not-used";
  config.authCode = "not-used";
  config.tradeFront = "tcp://127.0.0.1:30001";

  CtpTraderSession session(config);
  CtpQueryService queries(session);
  Recorder recorder;
  queries.setSink(&recorder);

  // Not started, so not READY: the query must fail instead of reporting a zero
  // account or an empty position list.
  check(!queries.queryAccount(std::chrono::milliseconds(1)), "query before READY fails");
  check(queries.lastError().find("READY") != std::string::npos,
        "failure names the required state");
  check(!queries.queryPositions(std::chrono::milliseconds(1)), "position query before READY fails");
  check(recorder.published == 0, "a failed query publishes no snapshot");
}

}  // namespace

int main() {
  checkBatchHeader();
  checkPositionSentinels();
  checkAccountMapping();
  checkOrderAndTradeMapping();
  checkFailurePublishesNothing();

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_query: batch header, row mapping, and silent failure\n";
  return 0;
}

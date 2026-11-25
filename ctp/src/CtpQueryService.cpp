#include "ors/ctp/CtpQueryService.h"
#include "ors/ctp/PriceSanitize.h"

#include <ThostFtdcTraderApi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
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

/// Quantity-weighted mean of two prices; 0 when neither side has quantity.
double weightedPrice(double leftPrice, int leftQty, double rightPrice, int rightQty) {
  const int total = leftQty + rightQty;
  if (total <= 0) {
    return 0.0;
  }
  return (leftPrice * leftQty + rightPrice * rightQty) / total;
}

}  // namespace

AccountSnapshot mapAccountSnapshot(const CThostFtdcTradingAccountField& row,
                                   const SnapshotHeader& header) {
  AccountSnapshot snapshot;
  snapshot.header = header;
  snapshot.balance = sanitizeMoney(row.Balance);
  snapshot.available = sanitizeMoney(row.Available);
  snapshot.margin = sanitizeMoney(row.CurrMargin);
  snapshot.frozenMargin = sanitizeMoney(row.FrozenMargin);
  snapshot.commission = sanitizeMoney(row.Commission);
  snapshot.closeProfit = sanitizeMoney(row.CloseProfit);
  snapshot.positionProfit = sanitizeMoney(row.PositionProfit);
  snapshot.deposit = sanitizeMoney(row.Deposit);
  snapshot.withdraw = sanitizeMoney(row.Withdraw);
  return snapshot;
}

PositionSnapshot mapPositionSnapshot(const CThostFtdcInvestorPositionField& row,
                                     const SnapshotHeader& header, int volumeMultiple) {
  PositionSnapshot snapshot;
  snapshot.header = header;
  snapshot.instrumentId = text(row.InstrumentID);
  snapshot.exchange = text(row.ExchangeID);

  const int position = row.Position;
  const int today = row.TodayPosition;
  const int yesterday = row.YdPosition != 0 ? row.YdPosition : position - today;
  const double cost = sanitizeMoney(row.PositionCost);
  const double avgPrice = position > 0 && volumeMultiple > 0
                              ? cost / (static_cast<double>(position) * volumeMultiple)
                              : 0.0;
  snapshot.settlementPrice = sanitizePrice(row.SettlementPrice);

  // shortcut: some venues also report net positions (PosiDirection 'N'); SimNow
  // futures only report long and short, so a net row is counted as long until a
  // venue that produces it needs a real split.
  if (row.PosiDirection == THOST_FTDC_PD_Short) {
    snapshot.shortQty = position;
    snapshot.todayShort = today;
    snapshot.yesterdayShort = yesterday;
    snapshot.shortAvgPrice = avgPrice;
  } else {
    snapshot.longQty = position;
    snapshot.todayLong = today;
    snapshot.yesterdayLong = yesterday;
    snapshot.longAvgPrice = avgPrice;
  }
  return snapshot;
}

OrderSnapshot mapOrderSnapshot(const CThostFtdcOrderField& row,
                               const SnapshotHeader& header) {
  OrderSnapshot snapshot;
  snapshot.header = header;
  snapshot.orderRef = text(row.OrderRef);
  snapshot.orderSysId = text(row.OrderSysID);
  snapshot.instrumentId = text(row.InstrumentID);
  snapshot.exchange = text(row.ExchangeID);
  snapshot.direction = row.Direction;
  snapshot.offsetFlag = row.CombOffsetFlag[0];
  snapshot.orderStatus = row.OrderStatus;
  snapshot.volumeTraded = row.VolumeTraded;
  snapshot.volumeTotal = row.VolumeTotal;
  snapshot.volumeTotalOriginal = row.VolumeTotalOriginal;
  snapshot.limitPrice = sanitizePrice(row.LimitPrice);
  snapshot.frontId = row.FrontID;
  snapshot.sessionId = row.SessionID;
  return snapshot;
}

TradeSnapshot mapTradeSnapshot(const CThostFtdcTradeField& row,
                               const SnapshotHeader& header) {
  TradeSnapshot snapshot;
  snapshot.header = header;
  snapshot.tradeId = text(row.TradeID);
  snapshot.orderRef = text(row.OrderRef);
  snapshot.orderSysId = text(row.OrderSysID);
  snapshot.instrumentId = text(row.InstrumentID);
  snapshot.exchange = text(row.ExchangeID);
  snapshot.direction = row.Direction;
  snapshot.offsetFlag = row.OffsetFlag;
  snapshot.volume = row.Volume;
  snapshot.price = sanitizePrice(row.Price);
  snapshot.tradeDate = text(row.TradeDate);
  snapshot.tradeTime = text(row.TradeTime);
  return snapshot;
}

std::vector<PositionSnapshot> mergePositions(std::vector<PositionSnapshot> rows) {
  std::vector<PositionSnapshot> merged;
  std::unordered_map<std::string, std::size_t> byInstrument;
  for (auto& row : rows) {
    const auto found = byInstrument.find(row.instrumentId);
    if (found == byInstrument.end()) {
      byInstrument.emplace(row.instrumentId, merged.size());
      merged.push_back(std::move(row));
      continue;
    }
    PositionSnapshot& target = merged[found->second];
    target.longAvgPrice =
        weightedPrice(target.longAvgPrice, target.longQty, row.longAvgPrice, row.longQty);
    target.shortAvgPrice =
        weightedPrice(target.shortAvgPrice, target.shortQty, row.shortAvgPrice, row.shortQty);
    target.longQty += row.longQty;
    target.shortQty += row.shortQty;
    target.todayLong += row.todayLong;
    target.todayShort += row.todayShort;
    target.yesterdayLong += row.yesterdayLong;
    target.yesterdayShort += row.yesterdayShort;
    if (target.settlementPrice == 0.0) {
      target.settlementPrice = row.settlementPrice;
    }
    if (target.exchange.empty()) {
      target.exchange = row.exchange;
    }
  }

  merged.erase(std::remove_if(merged.begin(), merged.end(),
                              [](const PositionSnapshot& row) {
                                return row.longQty == 0 && row.shortQty == 0;
                              }),
               merged.end());
  for (auto& row : merged) {
    row.avgPrice = weightedPrice(row.longAvgPrice, row.longQty, row.shortAvgPrice, row.shortQty);
  }
  return merged;
}

struct CtpQueryService::Impl {
  explicit Impl(CtpTraderSession& session) : session_(session) {
    spi_ = std::make_unique<QuerySpi>(*this);
    session_.setQuerySpi(spi_.get());
  }

  ~Impl() { session_.setQuerySpi(nullptr); }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  enum class Kind { None, Account, Position, Order, Trade };

  bool queryAccount(std::chrono::milliseconds timeout) {
    return run(Kind::Account, timeout);
  }

  bool queryPositions(std::chrono::milliseconds timeout) {
    return run(Kind::Position, timeout);
  }

  bool queryOrders(std::chrono::milliseconds timeout) {
    return run(Kind::Order, timeout);
  }

  bool queryTrades(std::chrono::milliseconds timeout) {
    return run(Kind::Trade, timeout);
  }

  void setSink(SnapshotSink* sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = sink;
  }

  std::string lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
  }

  bool run(Kind kind, std::chrono::milliseconds timeout) {
    if (session_.state() != SessionState::Ready) {
      return fail("query requires READY, session is " +
                  std::string(toString(session_.state())));
    }
    CThostFtdcTraderApi* api = session_.traderApi();
    if (api == nullptr) {
      return fail("query: trader api is not initialized");
    }

    int requestId = 0;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (pending_ != Kind::None) {
        error_ = "a query is already running";
        return false;
      }
      pending_ = kind;
      done_ = false;
      error_.clear();
      requestId_ = session_.nextRequestId();
      requestId = requestId_;
    }

    const int result = send(*api, kind, requestId);
    if (result != 0) {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_ = Kind::None;
      error_ = "query rejected by CTP rc=" + std::to_string(result);
      return false;
    }

    bool finished = false;
    std::string failure;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      finished = cv_.wait_for(lock, timeout, [this] { return done_; });
      if (!finished) {
        // Late rows for this request are ignored from here on.
        pending_ = Kind::None;
        clearRowsLocked();
        error_ = "query timed out after " + std::to_string(timeout.count()) + "ms";
      }
      failure = error_;
      if (!failure.empty()) {
        clearRowsLocked();
      }
    }
    if (!finished || !failure.empty()) {
      return false;
    }
    return publish(kind);
  }

 private:
  bool fail(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = message;
    return false;
  }

  int send(CThostFtdcTraderApi& api, Kind kind, int requestId) {
    const CtpConfig& config = session_.config();
    switch (kind) {
      case Kind::Account: {
        CThostFtdcQryTradingAccountField request;
        std::memset(&request, 0, sizeof(request));
        copyField(request.BrokerID, config.brokerId);
        copyField(request.InvestorID, config.investorId);
        return api.ReqQryTradingAccount(&request, requestId);
      }
      case Kind::Position: {
        CThostFtdcQryInvestorPositionField request;
        std::memset(&request, 0, sizeof(request));
        copyField(request.BrokerID, config.brokerId);
        copyField(request.InvestorID, config.investorId);
        return api.ReqQryInvestorPosition(&request, requestId);
      }
      case Kind::Order: {
        CThostFtdcQryOrderField request;
        std::memset(&request, 0, sizeof(request));
        copyField(request.BrokerID, config.brokerId);
        copyField(request.InvestorID, config.investorId);
        return api.ReqQryOrder(&request, requestId);
      }
      case Kind::Trade: {
        CThostFtdcQryTradeField request;
        std::memset(&request, 0, sizeof(request));
        copyField(request.BrokerID, config.brokerId);
        copyField(request.InvestorID, config.investorId);
        return api.ReqQryTrade(&request, requestId);
      }
      case Kind::None:
        break;
    }
    return -1;
  }

  bool publish(Kind kind) {
    switch (kind) {
      case Kind::Account:
        return publishAccount();
      case Kind::Position:
        return publishPositions();
      case Kind::Order:
        return publishOrders();
      case Kind::Trade:
        return publishTrades();
      case Kind::None:
        break;
    }
    return false;
  }

  bool publishAccount() {
    std::vector<CThostFtdcTradingAccountField> rows;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      rows.swap(accountRows_);
    }
    if (rows.empty()) {
      // An account query that returns nothing is a failure, not a zero balance.
      return fail("account query returned no rows");
    }
    if (rows.size() > 1) {
      std::cerr << "[ctp] account query returned " << rows.size()
                << " rows, using the first\n";
    }
    const SnapshotHeader header = makeHeader("ACC", text(rows.front().TradingDay));
    const AccountSnapshot snapshot = mapAccountSnapshot(rows.front(), header);
    SnapshotSink* sink = currentSink();
    if (sink != nullptr) {
      sink->onSnapshot(snapshot);
    }
    return true;
  }

  bool publishPositions() {
    std::vector<CThostFtdcInvestorPositionField> rows;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      rows.swap(positionRows_);
    }
    const SnapshotHeader header =
        makeHeader("POS", rows.empty() ? std::string() : text(rows.front().TradingDay));
    std::vector<PositionSnapshot> snapshots;
    snapshots.reserve(rows.size());
    for (const auto& row : rows) {
      const std::string instrumentId = text(row.InstrumentID);
      snapshots.push_back(
          mapPositionSnapshot(row, header, session_.volumeMultiple(instrumentId)));
    }
    snapshots = mergePositions(std::move(snapshots));
    SnapshotSink* sink = currentSink();
    if (sink != nullptr) {
      sink->onSnapshot(snapshots);
    }
    return true;
  }

  bool publishOrders() {
    std::vector<CThostFtdcOrderField> rows;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      rows.swap(orderRows_);
    }
    const SnapshotHeader header =
        makeHeader("ORD", rows.empty() ? std::string() : text(rows.front().TradingDay));
    ReconciliationSnapshot snapshot;
    snapshot.orders.reserve(rows.size());
    for (const auto& row : rows) {
      snapshot.orders.push_back(mapOrderSnapshot(row, header));
    }
    SnapshotSink* sink = currentSink();
    if (sink != nullptr) {
      sink->onSnapshot(snapshot);
    }
    return true;
  }

  bool publishTrades() {
    std::vector<CThostFtdcTradeField> rows;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      rows.swap(tradeRows_);
    }
    const SnapshotHeader header =
        makeHeader("TRD", rows.empty() ? std::string() : text(rows.front().TradingDay));
    ReconciliationSnapshot snapshot;
    snapshot.trades.reserve(rows.size());
    for (const auto& row : rows) {
      snapshot.trades.push_back(mapTradeSnapshot(row, header));
    }
    SnapshotSink* sink = currentSink();
    if (sink != nullptr) {
      sink->onSnapshot(snapshot);
    }
    return true;
  }

  /// One header per query: every row of the batch copies these values.
  SnapshotHeader makeHeader(const char* tag, const std::string& rowTradingDay) {
    SnapshotHeader header;
    header.accountId = session_.config().investorId;
    header.tradingDay = session_.tradingDay();
    if (header.tradingDay.empty()) {
      header.tradingDay = rowTradingDay;
    }
    header.snapshotTimestamp = epochNanos();
    std::uint64_t sequence = 0;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sequence = ++sequence_;
    }
    header.snapshotId = (header.tradingDay.empty() ? std::string("unknown-day")
                                                   : header.tradingDay) +
                        "-" + tag + "-" + std::to_string(sequence);
    return header;
  }

  SnapshotSink* currentSink() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sink_;
  }

  void clearRowsLocked() {
    accountRows_.clear();
    positionRows_.clear();
    orderRows_.clear();
    tradeRows_.clear();
  }

  void finishLocked() {
    pending_ = Kind::None;
    done_ = true;
    cv_.notify_all();
  }

  std::string errorText(const CThostFtdcRspInfoField* info) {
    if (info == nullptr) {
      return "no error info";
    }
    return std::to_string(info->ErrorID) + " " + text(info->ErrorMsg);
  }

  /// Ends the pending query with an error; all buffered rows are dropped.
  void failPending(const CThostFtdcRspInfoField* info) {  // caller holds the lock
    error_ = "query failed: " + errorText(info);
    pending_ = Kind::None;
    done_ = true;
    clearRowsLocked();
    cv_.notify_all();
  }

  /// True when this response belongs to the pending query and is not an error.
  bool accept(Kind kind, const CThostFtdcRspInfoField* info, int requestId) {
    if (pending_ != kind || requestId != requestId_) {
      return false;
    }
    if (info != nullptr && info->ErrorID != 0) {
      failPending(info);
      return false;
    }
    return true;
  }

  void onAccountRow(const CThostFtdcTradingAccountField* row,
                    const CThostFtdcRspInfoField* info, int requestId, bool isLast) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accept(Kind::Account, info, requestId)) {
      return;
    }
    if (row != nullptr) {
      accountRows_.push_back(*row);
    }
    if (isLast) {
      finishLocked();
    }
  }

  void onPositionRow(const CThostFtdcInvestorPositionField* row,
                     const CThostFtdcRspInfoField* info, int requestId, bool isLast) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accept(Kind::Position, info, requestId)) {
      return;
    }
    if (row != nullptr) {
      positionRows_.push_back(*row);
    }
    if (isLast) {
      finishLocked();
    }
  }

  void onOrderRow(const CThostFtdcOrderField* row, const CThostFtdcRspInfoField* info,
                  int requestId, bool isLast) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accept(Kind::Order, info, requestId)) {
      return;
    }
    if (row != nullptr) {
      orderRows_.push_back(*row);
    }
    if (isLast) {
      finishLocked();
    }
  }

  void onTradeRow(const CThostFtdcTradeField* row, const CThostFtdcRspInfoField* info,
                  int requestId, bool isLast) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!accept(Kind::Trade, info, requestId)) {
      return;
    }
    if (row != nullptr) {
      tradeRows_.push_back(*row);
    }
    if (isLast) {
      finishLocked();
    }
  }

  void onRspError(const CThostFtdcRspInfoField* info, int requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_ == Kind::None || requestId != requestId_) {
      return;
    }
    failPending(info);
  }

  struct QuerySpi final : CThostFtdcTraderSpi {
    explicit QuerySpi(Impl& owner) : owner_(owner) {}

    void OnRspQryTradingAccount(CThostFtdcTradingAccountField* account,
                                CThostFtdcRspInfoField* info, int requestId,
                                bool isLast) override {
      owner_.onAccountRow(account, info, requestId, isLast);
    }

    void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* position,
                                  CThostFtdcRspInfoField* info, int requestId,
                                  bool isLast) override {
      owner_.onPositionRow(position, info, requestId, isLast);
    }

    void OnRspQryOrder(CThostFtdcOrderField* order, CThostFtdcRspInfoField* info,
                       int requestId, bool isLast) override {
      owner_.onOrderRow(order, info, requestId, isLast);
    }

    void OnRspQryTrade(CThostFtdcTradeField* trade, CThostFtdcRspInfoField* info,
                       int requestId, bool isLast) override {
      owner_.onTradeRow(trade, info, requestId, isLast);
    }

    void OnRspError(CThostFtdcRspInfoField* info, int requestId, bool) override {
      owner_.onRspError(info, requestId);
    }

    Impl& owner_;
  };

  CtpTraderSession& session_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  Kind pending_{Kind::None};
  bool done_{false};
  int requestId_{0};
  std::string error_;
  std::uint64_t sequence_{0};
  SnapshotSink* sink_{nullptr};
  std::vector<CThostFtdcTradingAccountField> accountRows_;
  std::vector<CThostFtdcInvestorPositionField> positionRows_;
  std::vector<CThostFtdcOrderField> orderRows_;
  std::vector<CThostFtdcTradeField> tradeRows_;
  std::unique_ptr<QuerySpi> spi_;
};

CtpQueryService::CtpQueryService(CtpTraderSession& session)
    : impl_(std::make_unique<Impl>(session)) {}

CtpQueryService::~CtpQueryService() = default;

bool CtpQueryService::queryAccount(std::chrono::milliseconds timeout) {
  return impl_->queryAccount(timeout);
}

bool CtpQueryService::queryPositions(std::chrono::milliseconds timeout) {
  return impl_->queryPositions(timeout);
}

bool CtpQueryService::queryOrders(std::chrono::milliseconds timeout) {
  return impl_->queryOrders(timeout);
}

bool CtpQueryService::queryTrades(std::chrono::milliseconds timeout) {
  return impl_->queryTrades(timeout);
}

void CtpQueryService::setSink(SnapshotSink* sink) {
  impl_->setSink(sink);
}

std::string CtpQueryService::lastError() const {
  return impl_->lastError();
}

}  // namespace ors::ctp

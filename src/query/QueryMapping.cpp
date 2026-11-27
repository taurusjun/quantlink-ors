#include "query/QueryMapping.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace ors {
namespace {

double finiteOrZero(double value) {
  return std::isfinite(value) && value != std::numeric_limits<double>::max()
             ? value
             : 0.0;
}

}  // namespace

AccountSnapshot mapAccountSnapshot(const CThostFtdcTradingAccountField& row,
                                   std::uint64_t requestId,
                                   const OrsConfig& config) {
  AccountSnapshot snapshot;
  snapshot.requestId = requestId;
  snapshot.accountId = config.accountId;
  snapshot.tradingDay = row.TradingDay;
  snapshot.balance = finiteOrZero(row.Balance);
  snapshot.available = finiteOrZero(row.Available);
  snapshot.margin = finiteOrZero(row.CurrMargin);
  snapshot.frozenMargin = finiteOrZero(row.FrozenMargin);
  snapshot.commission = finiteOrZero(row.Commission);
  snapshot.closeProfit = finiteOrZero(row.CloseProfit);
  snapshot.positionProfit = finiteOrZero(row.PositionProfit);
  snapshot.deposit = finiteOrZero(row.Deposit);
  snapshot.withdraw = finiteOrZero(row.Withdraw);
  snapshot.timestamp = 0;
  return snapshot;
}

}  // namespace ors

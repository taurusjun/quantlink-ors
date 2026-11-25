#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "ors/ctp/CtpAdapter.h"
#include "ors/ctp/CtpQueryService.h"

namespace ors::ctp {

/// Tracks order recovery state across reconnect epochs. It does not own the
/// venue session or the strategy position; it only decides what is safe to
/// submit after a reconnect and what the control plane must reconcile.
class CtpReconciler {
 public:
  enum class State { Unknown, Open, Filled, Canceled, Rejected };

  CtpReconciler();

  /// Sets the epoch for a fresh runtime start.
  void setEpoch(std::uint64_t epoch);

  /// Starts a new epoch. Open orders become UNKNOWN until the query returns.
  void onReconnect();

  /// Applies a live report only when it belongs to the current epoch.
  void observe(const OrderUpdate& update);
  void observe(const Trade& trade);

  /// Applies the venue query snapshot. Local open orders not returned by the
  /// query become UNKNOWN; terminal states are never downgraded.
  void reconcile(const std::vector<OrderSnapshot>& orders,
                 const std::vector<TradeSnapshot>& trades,
                 const std::vector<PositionSnapshot>& positions);

  State state(std::uint64_t orderRef) const;
  bool canSubmit(std::uint64_t orderRef) const;
  std::uint64_t epoch() const;

 private:
  static State fromOrderStatus(char status);
  static bool terminal(State state);
  static std::uint64_t parseOrderRef(const std::string& value);

  std::uint64_t epoch_{1};
  std::map<std::uint64_t, State> states_;
};

}  // namespace ors::ctp

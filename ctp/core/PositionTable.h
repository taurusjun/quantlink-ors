#pragma once

#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace ors {

struct ContractPosition {
  int onLongPos{0};
  int todayLongPos{0};
  int onShortPos{0};
  int todayShortPos{0};
};

enum class PositionSide { Long, Short };

struct PositionRow {
  std::string instrumentId;
  PositionSide side{PositionSide::Long};
  int position{0};
  int todayPosition{0};
};

enum class PositionAction {
  CloseToday = 1,
  CloseYesterday = 2,
  Open = 3,
};

struct OffsetReservation {
  PositionAction action{PositionAction::Open};
  bool valid{false};
};

/// Account-level position book. It is not a query snapshot: it is updated by
/// startup queries, order reservation, failure rollback and fills.
class PositionTable {
 public:
  void replaceFromQuery(std::vector<PositionRow> rows);
  ContractPosition get(std::string_view symbol) const;
  bool empty() const;

  /// Mirrors company SetCombOffsetFlag: reserve the close quantity before the
  /// order is sent.
  OffsetReservation reserveForOrder(std::string_view symbol, char side, int qty,
                                    bool shfeCloseToday);
  void restoreOnFailure(std::string_view symbol, char side,
                        PositionAction action, int qty);
  void applyTrade(std::string_view symbol, char side, PositionAction action,
                  int qty);

 private:
  ContractPosition& at(std::string_view symbol);

  mutable std::mutex mutex_;
  std::map<std::string, ContractPosition> positions_;
};

}  // namespace ors

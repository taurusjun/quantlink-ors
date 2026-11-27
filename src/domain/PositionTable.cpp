#include "domain/PositionTable.h"

#include <utility>

namespace ors {

void PositionTable::replaceFromQuery(std::vector<PositionRow> rows) {
  std::map<std::string, ContractPosition> fresh;
  for (const PositionRow& row : rows) {
    if (row.position <= 0) {
      continue;
    }
    ContractPosition& position = fresh[row.instrumentId];
    const int today = row.todayPosition < 0 ? 0 : row.todayPosition;
    const int yesterday = row.position - today;
    if (row.side == PositionSide::Long) {
      position.todayLongPos += today;
      position.onLongPos += yesterday;
    } else {
      position.todayShortPos += today;
      position.onShortPos += yesterday;
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);
  positions_.swap(fresh);
}

ContractPosition PositionTable::get(std::string_view symbol) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = positions_.find(std::string(symbol));
  return found == positions_.end() ? ContractPosition{} : found->second;
}

bool PositionTable::empty() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return positions_.empty();
}

OffsetReservation PositionTable::reserveForOrder(std::string_view symbol,
                                                 char side, int qty,
                                                 bool shfeCloseToday) {
  if (qty <= 0) {
    return {};
  }

  std::lock_guard<std::mutex> lock(mutex_);
  ContractPosition& position = at(symbol);
  if (side == 'B') {
    if (qty <= position.todayShortPos) {
      position.todayShortPos -= qty;
      return {PositionAction::CloseToday, true};
    }
    if (qty <= position.onShortPos) {
      position.onShortPos -= qty;
      return {PositionAction::CloseYesterday, true};
    }
    return {PositionAction::Open, true};
  }
  if (side == 'S') {
    if (qty <= position.todayLongPos) {
      position.todayLongPos -= qty;
      return {PositionAction::CloseToday, true};
    }
    if (qty <= position.onLongPos) {
      position.onLongPos -= qty;
      return {PositionAction::CloseYesterday, true};
    }
    return {PositionAction::Open, true};
  }
  (void)shfeCloseToday;
  return {};
}

void PositionTable::restoreOnFailure(std::string_view symbol, char side,
                                     PositionAction action, int qty) {
  if (qty <= 0 || action == PositionAction::Open) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  ContractPosition& position = at(symbol);
  if (action == PositionAction::CloseToday) {
    if (side == 'B') {
      position.todayShortPos += qty;
    } else if (side == 'S') {
      position.todayLongPos += qty;
    }
  } else if (action == PositionAction::CloseYesterday) {
    if (side == 'B') {
      position.onShortPos += qty;
    } else if (side == 'S') {
      position.onLongPos += qty;
    }
  }
}

void PositionTable::applyTrade(std::string_view symbol, char side,
                               PositionAction action, int qty) {
  if (qty <= 0 || action != PositionAction::Open) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  ContractPosition& position = at(symbol);
  if (side == 'B') {
    position.todayLongPos += qty;
  } else if (side == 'S') {
    position.todayShortPos += qty;
  }
}

ContractPosition& PositionTable::at(std::string_view symbol) {
  return positions_[std::string(symbol)];
}

}  // namespace ors

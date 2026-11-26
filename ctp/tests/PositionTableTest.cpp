#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "core/PositionTable.h"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

::ors::PositionRow row(std::string symbol, ::ors::PositionSide side, int position,
                     int today) {
  return {std::move(symbol), side, position, today};
}

void replaceMergesLongAndShort() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 7, 2),
                          row("au2612", ::ors::PositionSide::Short, 3, 1)});
  const auto position = table.get("au2612");
  check(position.todayLongPos == 2 && position.onLongPos == 5,
        "long row split");
  check(position.todayShortPos == 1 && position.onShortPos == 2,
        "short row split");
}

void emptyQueryClearsPreviousPositions() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 1, 1)});
  table.replaceFromQuery({});
  check(table.empty(), "empty query clears the table");
  check(table.get("au2612").todayLongPos == 0, "missing symbol is zero");
}

void reserveCloseTodayDeductsTodayQuantity() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 5, 3)});
  const auto reservation = table.reserveForOrder("au2612", 'S', 2, true);
  check(reservation.action == ::ors::PositionAction::CloseToday,
        "today long reserves CloseToday");
  const auto position = table.get("au2612");
  check(position.todayLongPos == 1 && position.onLongPos == 2,
        "today quantity is deducted");
}

void reserveCloseYesterdayDeductsYesterdayQuantity() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 5, 1)});
  const auto reservation = table.reserveForOrder("au2612", 'S', 2, true);
  check(reservation.action == ::ors::PositionAction::CloseYesterday,
        "yesterday long reserves CloseYesterday");
  const auto position = table.get("au2612");
  check(position.todayLongPos == 1 && position.onLongPos == 2,
        "yesterday quantity is deducted");
}

void reserveOpenLeavesPositionUnchanged() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 1, 1)});
  const auto reservation = table.reserveForOrder("au2612", 'S', 2, true);
  check(reservation.action == ::ors::PositionAction::Open, "excess is Open");
  const auto position = table.get("au2612");
  check(position.todayLongPos == 1 && position.onLongPos == 0,
        "open does not deduct");
}

void orderErrorRestoresReservedQuantity() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 5, 2)});
  table.reserveForOrder("au2612", 'S', 2, true);
  table.restoreOnFailure("au2612", 'S', ::ors::PositionAction::CloseToday, 2);
  const auto position = table.get("au2612");
  check(position.todayLongPos == 2, "today close is restored");
}

void cancelRestoresYesterdayQuantity() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 5, 1)});
  table.reserveForOrder("au2612", 'S', 2, true);
  table.restoreOnFailure("au2612", 'S', ::ors::PositionAction::CloseYesterday, 2);
  const auto position = table.get("au2612");
  check(position.onLongPos == 4, "yesterday close is restored");
}

void openTradeAddsTodayQuantity() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 0, 0)});
  table.applyTrade("au2612", 'B', ::ors::PositionAction::Open, 2);
  const auto position = table.get("au2612");
  check(position.todayLongPos == 2, "open buy adds today long");
}

void closeTradeDoesNotDoubleCount() {
  ::ors::PositionTable table;
  table.replaceFromQuery({row("au2612", ::ors::PositionSide::Long, 5, 2)});
  table.reserveForOrder("au2612", 'S', 2, true);
  table.applyTrade("au2612", 'S', ::ors::PositionAction::CloseToday, 2);
  const auto position = table.get("au2612");
  check(position.todayLongPos == 0 && position.onLongPos == 3,
        "close trade does not add again");
}

}  // namespace

int main() {
  replaceMergesLongAndShort();
  emptyQueryClearsPreviousPositions();
  reserveCloseTodayDeductsTodayQuantity();
  reserveCloseYesterdayDeductsYesterdayQuantity();
  reserveOpenLeavesPositionUnchanged();
  orderErrorRestoresReservedQuantity();
  cancelRestoresYesterdayQuantity();
  openTradeAddsTodayQuantity();
  closeTradeDoesNotDoubleCount();
  return failures == 0 ? 0 : 1;
}

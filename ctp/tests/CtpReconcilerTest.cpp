// CtpReconciler tests: reconnect epoch isolation, query reconciliation,
// missing orders becoming UNKNOWN, and duplicate-submit protection.

#include <iostream>
#include <string>
#include <vector>

#include "ors/ctp/CtpReconciler.h"

namespace {

using ors::ctp::CtpReconciler;
using ors::ctp::OrderSnapshot;
using ors::ctp::OrderUpdate;
using ors::ctp::OrderStatus;
using ors::ctp::SnapshotHeader;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

OrderUpdate update(std::uint64_t ref, std::uint64_t epoch, OrderStatus status) {
  OrderUpdate value;
  value.orderRef = ref;
  value.sourcePublicationEpoch = epoch;
  value.status = status;
  return value;
}

OrderSnapshot snapshot(std::uint64_t ref, char status) {
  OrderSnapshot value;
  value.header.accountId = "ACC";
  value.header.tradingDay = "20261124";
  value.header.snapshotId = "snap";
  value.header.snapshotTimestamp = 1;
  value.header.staleAfterMs = 5000;
  value.orderRef = std::to_string(ref);
  value.orderStatus = status;
  value.volumeTotalOriginal = 1;
  value.volumeTraded = status == '0' ? 1 : 0;
  return value;
}

void checkReconnectDropsOldReports() {
  CtpReconciler reconciler;
  reconciler.observe(update(1, 1, OrderStatus::Ack));
  check(reconciler.state(1) == CtpReconciler::State::Open, "initial ACK is open");

  reconciler.onReconnect();
  check(reconciler.epoch() == 2, "epoch advances on reconnect");
  reconciler.observe(update(1, 1, OrderStatus::Filled));
  check(reconciler.state(1) == CtpReconciler::State::Unknown,
        "late old-session fill is ignored");
}

void checkQueryFilledWins() {
  CtpReconciler reconciler;
  reconciler.onReconnect();
  reconciler.observe(update(7, 2, OrderStatus::Ack));
  check(reconciler.canSubmit(7) == false, "open order cannot be submitted again");

  reconciler.reconcile({snapshot(7, '0')}, {}, {});
  check(reconciler.state(7) == CtpReconciler::State::Filled,
        "query FILLED becomes terminal");
  check(reconciler.canSubmit(7), "a terminal order may be reused");
}

void checkMissingOrderBecomesUnknown() {
  CtpReconciler reconciler;
  reconciler.onReconnect();
  reconciler.observe(update(8, 2, OrderStatus::Ack));
  reconciler.reconcile({}, {}, {});
  check(reconciler.state(8) == CtpReconciler::State::Unknown,
        "an unreturned open order becomes UNKNOWN");
  check(reconciler.canSubmit(8) == false,
        "an UNKNOWN order must not be submitted again");
}

}  // namespace

int main() {
  checkReconnectDropsOldReports();
  checkQueryFilledWins();
  checkMissingOrderBecomesUnknown();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "reconciler tests passed\n";
  return 0;
}

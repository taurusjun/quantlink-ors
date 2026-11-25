#include "ors/ctp/CtpReconciler.h"

#include <set>
#include <stdexcept>
#include <string>

namespace ors::ctp {

CtpReconciler::CtpReconciler() = default;

void CtpReconciler::setEpoch(std::uint64_t epoch) {
  if (epoch > 0) epoch_ = epoch;
}

void CtpReconciler::onReconnect() {
  ++epoch_;
  for (auto& [orderRef, state] : states_) {
    if (state == State::Open) state = State::Unknown;
  }
}

void CtpReconciler::observe(const OrderUpdate& update) {
  if (update.sourcePublicationEpoch != epoch_) return;
  if (update.orderRef == 0) return;
  switch (update.status) {
    case OrderStatus::Filled:
      states_[update.orderRef] = State::Filled;
      break;
    case OrderStatus::Canceled:
      states_[update.orderRef] = State::Canceled;
      break;
    case OrderStatus::Rejected:
      states_[update.orderRef] = State::Rejected;
      break;
    case OrderStatus::New:
    case OrderStatus::Ack:
    case OrderStatus::Partial:
      if (!terminal(states_[update.orderRef])) {
        states_[update.orderRef] = State::Open;
      }
      break;
  }
}

void CtpReconciler::observe(const Trade& trade) {
  if (trade.sourcePublicationEpoch != epoch_ || trade.orderRef == 0) return;
  if (!terminal(states_[trade.orderRef])) {
    // A trade proves at least a partial fill; the following OrderUpdate or
    // query decides whether it is terminal.
    states_[trade.orderRef] = State::Open;
  }
}

void CtpReconciler::reconcile(const std::vector<OrderSnapshot>& orders,
                              const std::vector<TradeSnapshot>& trades,
                              const std::vector<PositionSnapshot>& positions) {
  (void)positions;
  std::set<std::uint64_t> seen;
  for (const auto& order : orders) {
    const std::uint64_t orderRef = parseOrderRef(order.orderRef);
    if (orderRef == 0) continue;
    seen.insert(orderRef);
    states_[orderRef] = fromOrderStatus(order.orderStatus);
  }
  for (const auto& trade : trades) {
    const std::uint64_t orderRef = parseOrderRef(trade.orderRef);
    if (orderRef == 0) continue;
    seen.insert(orderRef);
    if (!terminal(states_[orderRef])) states_[orderRef] = State::Filled;
  }
  for (auto& [orderRef, state] : states_) {
    if (state == State::Open && seen.find(orderRef) == seen.end()) {
      state = State::Unknown;
    }
  }
}

CtpReconciler::State CtpReconciler::state(std::uint64_t orderRef) const {
  const auto found = states_.find(orderRef);
  return found == states_.end() ? State::Unknown : found->second;
}

bool CtpReconciler::canSubmit(std::uint64_t orderRef) const {
  const auto found = states_.find(orderRef);
  return found == states_.end() || terminal(found->second);
}

std::uint64_t CtpReconciler::epoch() const {
  return epoch_;
}

CtpReconciler::State CtpReconciler::fromOrderStatus(char status) {
  // CTP: '0' AllTraded, '5' Canceled; other active states are OPEN.
  if (status == '0') return State::Filled;
  if (status == '5') return State::Canceled;
  return State::Open;
}

bool CtpReconciler::terminal(State state) {
  return state == State::Filled || state == State::Canceled || state == State::Rejected;
}

std::uint64_t CtpReconciler::parseOrderRef(const std::string& value) {
  try {
    return std::stoull(value);
  } catch (const std::exception&) {
    return 0;
  }
}

}  // namespace ors::ctp

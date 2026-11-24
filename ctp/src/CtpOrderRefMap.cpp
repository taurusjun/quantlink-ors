#include "ors/ctp/CtpOrderRefMap.h"

#include <utility>

namespace ors::ctp {

std::string CtpOrderRefMap::ctpRef(std::uint64_t orderRef) const {
  const auto found = byOrderRef_.find(orderRef);
  return found == byOrderRef_.end() ? std::string() : found->second.ctpRef;
}

bool CtpOrderRefMap::add(std::uint64_t orderRef) {
  if (orderRef == 0 || orderRef > kMaxCtpOrderRef) {
    return false;
  }
  if (byOrderRef_.find(orderRef) != byOrderRef_.end()) {
    return false;
  }
  Entry entry;
  entry.ctpRef = std::to_string(orderRef);
  byCtpRef_.emplace(entry.ctpRef, orderRef);
  byOrderRef_.emplace(orderRef, std::move(entry));
  return true;
}

bool CtpOrderRefMap::bind(std::uint64_t orderRef, int frontId, int sessionId) {
  const auto found = byOrderRef_.find(orderRef);
  if (found == byOrderRef_.end() || frontId == 0 || sessionId == 0) {
    return false;
  }
  Entry& entry = found->second;
  if (entry.frontId != 0 &&
      (entry.frontId != frontId || entry.sessionId != sessionId)) {
    // A second session must not take over an order it did not accept.
    return false;
  }
  entry.frontId = frontId;
  entry.sessionId = sessionId;
  return true;
}

std::uint64_t CtpOrderRefMap::resolve(int frontId, int sessionId,
                                      const std::string& ctpRef) const {
  const auto found = byCtpRef_.find(ctpRef);
  if (found == byCtpRef_.end()) {
    return 0;
  }
  const Entry& entry = byOrderRef_.at(found->second);
  const bool frontMatches = frontId == 0 || entry.frontId == 0 || entry.frontId == frontId;
  const bool sessionMatches =
      sessionId == 0 || entry.sessionId == 0 || entry.sessionId == sessionId;
  return frontMatches && sessionMatches ? found->second : 0;
}

void CtpOrderRefMap::clear() {
  byOrderRef_.clear();
  byCtpRef_.clear();
}

}  // namespace ors::ctp

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace ors::ctp {

/// CTP OrderRef is a char[13]: at most twelve digits.
inline constexpr std::uint64_t kMaxCtpOrderRef = 999999999999ULL;

/**
 * Contract orderRef <-> CTP OrderRef and the (FrontID, SessionID) CTP assigned
 * to it.
 *
 * The contract orderRef is owned by L2; CTP needs its own numeric string, and
 * `ReqOrderAction` addresses an order by the triple (FrontID, SessionID,
 * OrderRef). A ref is registered once and never overwritten: a duplicate or an
 * out-of-range value is rejected instead of silently reusing an order identity.
 *
 * Not thread-safe; CtpAdapter serialises access.
 */
class CtpOrderRefMap {
 public:
  /// CTP OrderRef of a registered order; empty when it is not registered.
  std::string ctpRef(std::uint64_t orderRef) const;

  /// Registers a new orderRef. False for 0, more than twelve digits, or a
  /// value that is already registered.
  bool add(std::uint64_t orderRef);

  /// Binds the front/session that accepted the order. The first binding wins;
  /// false when the order is unknown, the ids are 0, or they conflict with an
  /// earlier binding.
  bool bind(std::uint64_t orderRef, int frontId, int sessionId);

  /// Contract orderRef behind a CTP report, 0 when unknown. A frontId or
  /// sessionId of 0 matches any value, because an insert response carries
  /// neither and may arrive before the first venue report.
  std::uint64_t resolve(int frontId, int sessionId, const std::string& ctpRef) const;

  /// Drops every mapping. Used on reconnect, where the old FrontID/SessionID
  /// stops meaning anything.
  void clear();

  /// Removes one live order mapping after its terminal event.
  void erase(std::uint64_t orderRef);

  std::size_t size() const { return byOrderRef_.size(); }

 private:
  struct Entry {
    std::string ctpRef;
    int frontId{0};
    int sessionId{0};
  };

  std::unordered_map<std::uint64_t, Entry> byOrderRef_;
  std::map<std::string, std::uint64_t> byCtpRef_;
};

}  // namespace ors::ctp

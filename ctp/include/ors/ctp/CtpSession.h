#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/CtpConfig.h"

namespace ors::ctp {

enum class SessionState {
  Disconnected,
  Connecting,
  Authenticating,
  LoggingIn,
  SettlementConfirming,
  QueryingInstruments,
  Ready,
  Failed,
};

/// Upper-case name as used by the design doc and the smoke output.
const char* toString(SessionState state);

struct Instrument {
  std::string instrumentId;
  std::string exchangeId;
  std::string productId;
  int volumeMultiple{0};
  double priceTick{0.0};
};

/**
 * One TraderApi session for one CTP account.
 *
 * start() drives DISCONNECTED -> CONNECTING -> AUTHENTICATING -> LOGGING_IN ->
 * SETTLEMENT_CONFIRMING -> QUERYING_INSTRUMENTS -> READY and blocks until the
 * session is READY, FAILED, or the timeout expires. Any CTP error is surfaced
 * through state() == Failed and lastError(); READY is never reported for a
 * half-finished handshake.
 */
class CtpTraderSession {
 public:
  explicit CtpTraderSession(CtpConfig config);
  ~CtpTraderSession();

  CtpTraderSession(const CtpTraderSession&) = delete;
  CtpTraderSession& operator=(const CtpTraderSession&) = delete;

  bool start(std::chrono::milliseconds timeout = std::chrono::seconds(30));
  SessionState state() const;
  std::string lastError() const;

  /// Re-runs the instrument query; allowed after settlement confirm and when
  /// already READY. Returns false when the request cannot be sent.
  bool queryInstruments();

  std::vector<Instrument> instruments() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ors::ctp

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "ors/ctp/CtpConfig.h"

// SDK types; only pointers cross this header, the CTP headers stay in the .cpp.
class CThostFtdcTraderApi;
class CThostFtdcTraderSpi;

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

/// CTP publishes five depth levels; the L0 contract carries twenty.
inline constexpr std::size_t kCtpDepthLevels = 5;
inline constexpr std::size_t kMarketDepthLevels = 20;

/**
 * The depth fields of a CTP tick, free of SDK types so the contract mapping
 * can be unit-tested.
 */
struct DepthTick {
  std::string instrumentId;
  std::string exchangeId;
  double lastPrice{0.0};
  int lastVolume{0};
  double turnover{0.0};
  double openInterest{0.0};
  std::string updateTime;
  int updateMillisec{0};
  std::array<double, kCtpDepthLevels> bidPrices{};
  std::array<int, kCtpDepthLevels> bidQtys{};
  std::array<double, kCtpDepthLevels> askPrices{};
  std::array<int, kCtpDepthLevels> askQtys{};
};

struct MarketData {
  std::string instrumentId;
  std::string exchange;
  double lastPrice{0.0};
  long long volume{0};
  double turnover{0.0};
  double openInterest{0.0};
  std::string updateTime;
  int updateMillisec{0};
  std::array<double, kMarketDepthLevels> bidPrices{};
  std::array<int, kMarketDepthLevels> bidQtys{};
  std::array<double, kMarketDepthLevels> askPrices{};
  std::array<int, kMarketDepthLevels> askQtys{};
};

/**
 * Copies the five CTP levels and leaves levels 6..20 at zero. A level CTP
 * never published stays absent instead of repeating a price that is not there.
 */
MarketData mapDepthMarketData(const DepthTick& tick);

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

  /// Settings this session was created with. Credentials are in there but are
  /// never logged; use CtpConfig::summary() for output.
  const CtpConfig& config() const;

  /// Trading day from the login response; empty before READY.
  std::string tradingDay() const;

  /// The single TraderApi behind this session; null before a successful
  /// start(). Request-only users send through it and receive their responses
  /// via setQuerySpi().
  CThostFtdcTraderApi* traderApi() const;

  /// Next id of this session's request counter, so every request on the one API
  /// stays unique.
  int nextRequestId();

  /// CTP keeps one SPI, so the responses the session does not consume itself
  /// are forwarded to this one. It must outlive the session or be cleared
  /// first; pass nullptr to detach.
  void setQuerySpi(CThostFtdcTraderSpi* spi);

  /// The order-related callbacks (insert/action responses, OnRtnOrder,
  /// OnRtnTrade and the OnErrRtn* pair) go to this SPI. Same lifetime rule as
  /// setQuerySpi(); both slots may be filled at the same time.
  void setOrderSpi(CThostFtdcTraderSpi* spi);

  /// Re-runs the instrument query; allowed after settlement confirm and when
  /// already READY. Returns false when the request cannot be sent.
  bool queryInstruments();

  std::vector<Instrument> instruments() const;

  /// Volume multiple of a listed instrument; 0 when it is not in the table.
  int volumeMultiple(const std::string& instrumentId) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * One MdApi session for the same CTP account.
 *
 * start() drives DISCONNECTED -> CONNECTING -> LOGGING_IN -> READY and blocks
 * until READY, FAILED, or the timeout expires. Ticks are mapped with
 * mapDepthMarketData(); waitForMarketData() lets a smoke run block for the
 * first one.
 */
class CtpMdSession {
 public:
  explicit CtpMdSession(CtpConfig config);
  ~CtpMdSession();

  CtpMdSession(const CtpMdSession&) = delete;
  CtpMdSession& operator=(const CtpMdSession&) = delete;

  bool start(std::chrono::milliseconds timeout = std::chrono::seconds(30));
  SessionState state() const;
  std::string lastError() const;

  /// Sends SubscribeMarketData when READY; called earlier the list is sent
  /// right after login. Returns false when the request cannot be sent.
  bool subscribe(const std::vector<std::string>& instruments);

  /// Blocks until the first tick arrives; false on timeout.
  bool waitForMarketData(std::chrono::milliseconds timeout);

  MarketData lastMarketData() const;
  std::size_t marketDataCount() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ors::ctp

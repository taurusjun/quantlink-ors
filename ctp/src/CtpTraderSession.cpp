#include "ors/ctp/CtpSession.h"

#include <ThostFtdcTraderApi.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ors::ctp {
namespace {

template <std::size_t N>
void copyField(char (&field)[N], const std::string& value) {
  std::snprintf(field, N, "%s", value.c_str());
}

bool hasError(const CThostFtdcRspInfoField* info) {
  return info != nullptr && info->ErrorID != 0;
}

std::string errorText(const CThostFtdcRspInfoField* info) {
  if (info == nullptr) {
    return "no error info";
  }
  return std::to_string(info->ErrorID) + " " + info->ErrorMsg;
}

std::string flowPathWithSlash(const std::string& path) {
  if (!path.empty() && path.back() != '/') {
    return path + "/";
  }
  return path;
}

}  // namespace

const char* toString(SessionState state) {
  switch (state) {
    case SessionState::Disconnected:
      return "DISCONNECTED";
    case SessionState::Connecting:
      return "CONNECTING";
    case SessionState::Authenticating:
      return "AUTHENTICATING";
    case SessionState::LoggingIn:
      return "LOGGING_IN";
    case SessionState::SettlementConfirming:
      return "SETTLEMENT_CONFIRMING";
    case SessionState::QueryingInstruments:
      return "QUERYING_INSTRUMENTS";
    case SessionState::Ready:
      return "READY";
    case SessionState::Failed:
      return "FAILED";
  }
  return "UNKNOWN";
}

struct CtpTraderSession::Impl {
  explicit Impl(CtpConfig config) : config_(std::move(config)) {}
  ~Impl() { stop(); }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  bool start(std::chrono::milliseconds timeout) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ != SessionState::Disconnected) {
        lastError_ = "session already started";
        return false;
      }
      state_ = SessionState::Connecting;
    }
    logState();

    std::error_code error;
    std::filesystem::create_directories(config_.traderFlowPath, error);
    if (error) {
      fail("cannot create trader flow path " + config_.traderFlowPath + ": " + error.message());
      return false;
    }

    CThostFtdcTraderApi* api = CThostFtdcTraderApi::CreateFtdcTraderApi(
        flowPathWithSlash(config_.traderFlowPath).c_str(), true);
    if (api == nullptr) {
      fail("CreateFtdcTraderApi returned null");
      return false;
    }
    spi_ = std::make_unique<TraderSpi>(*this);
    api->RegisterSpi(spi_.get());
    api->SubscribePrivateTopic(THOST_TERT_QUICK);
    api->SubscribePublicTopic(THOST_TERT_QUICK);
    api->RegisterFront(const_cast<char*>(config_.tradeFront.c_str()));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      api_ = api;
    }
    api->Init();

    bool ready = false;
    std::string failure;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait_for(lock, timeout, [this] {
        return state_ == SessionState::Ready || state_ == SessionState::Failed;
      });
      if (state_ == SessionState::Ready) {
        ready = true;
      } else {
        if (state_ != SessionState::Failed) {
          lastError_ = "login timed out in state " + std::string(toString(state_));
          state_ = SessionState::Failed;
        }
        failure = lastError_;
      }
    }
    if (!ready) {
      std::cerr << "[ctp] login failed: " << failure << '\n';
      stop();
    }
    return ready;
  }

  void stop() {
    CThostFtdcTraderApi* api = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
      api = api_;
      api_ = nullptr;
    }
    if (api != nullptr) {
      api->Release();
    }
    spi_.reset();
  }

  SessionState state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
  }

  std::string lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
  }

  std::vector<Instrument> instruments() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instruments_;
  }

  bool queryInstruments() {
    CThostFtdcTraderApi* api = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ != SessionState::SettlementConfirming && state_ != SessionState::Ready) {
        lastError_ = std::string("instrument query not allowed in state ") + toString(state_);
        return false;
      }
      instruments_.clear();
      state_ = SessionState::QueryingInstruments;
      api = api_;
    }
    logState();
    if (api == nullptr) {
      fail("instrument query: trader api is not initialized");
      return false;
    }
    CThostFtdcQryInstrumentField request;
    std::memset(&request, 0, sizeof(request));
    const int result = api->ReqQryInstrument(&request, ++requestId_);
    if (result != 0) {
      fail("ReqQryInstrument rejected rc=" + std::to_string(result));
      return false;
    }
    return true;
  }

  void onFrontConnected() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_ || state_ == SessionState::Failed || state_ == SessionState::Ready) {
        return;
      }
      state_ = SessionState::Authenticating;
    }
    logState();

    CThostFtdcTraderApi* api = apiLocked();
    if (api == nullptr) {
      return;
    }
    CThostFtdcReqAuthenticateField request;
    std::memset(&request, 0, sizeof(request));
    copyField(request.BrokerID, config_.brokerId);
    copyField(request.UserID, config_.investorId);
    copyField(request.AppID, config_.appId);
    copyField(request.AuthCode, config_.authCode);
    const int result = api->ReqAuthenticate(&request, ++requestId_);
    if (result != 0) {
      fail("ReqAuthenticate rejected rc=" + std::to_string(result));
    }
  }

  void onFrontDisconnected(int reason) {
    std::string message = "front disconnected reason=" + std::to_string(reason);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        return;
      }
      // A disconnect during the handshake is a failed login; after READY it
      // drops us out of READY so nothing can submit orders on a dead session.
      state_ = state_ == SessionState::Ready ? SessionState::Disconnected
                                             : SessionState::Failed;
      lastError_ = message;
    }
    std::cerr << "[ctp] " << message << '\n';
    cv_.notify_all();
  }

  void onRspAuthenticate(const CThostFtdcRspInfoField* info, bool isLast) {
    if (hasError(info)) {
      fail("authenticate failed: " + errorText(info));
      return;
    }
    if (!isLast) {
      return;
    }
    setState(SessionState::LoggingIn);

    CThostFtdcTraderApi* api = apiLocked();
    if (api == nullptr) {
      return;
    }
    CThostFtdcReqUserLoginField request;
    std::memset(&request, 0, sizeof(request));
    copyField(request.BrokerID, config_.brokerId);
    copyField(request.UserID, config_.investorId);
    copyField(request.Password, config_.password);
    copyField(request.UserProductInfo, "quantlink-ors");
    const int result = api->ReqUserLogin(&request, ++requestId_);
    if (result != 0) {
      fail("ReqUserLogin rejected rc=" + std::to_string(result));
    }
  }

  void onRspUserLogin(const CThostFtdcRspUserLoginField*,
                      const CThostFtdcRspInfoField* info, bool isLast) {
    if (hasError(info)) {
      fail("login failed: " + errorText(info));
      return;
    }
    if (!isLast) {
      return;
    }
    setState(SessionState::SettlementConfirming);

    CThostFtdcTraderApi* api = apiLocked();
    if (api == nullptr) {
      return;
    }
    CThostFtdcSettlementInfoConfirmField request;
    std::memset(&request, 0, sizeof(request));
    copyField(request.BrokerID, config_.brokerId);
    copyField(request.InvestorID, config_.investorId);
    const int result = api->ReqSettlementInfoConfirm(&request, ++requestId_);
    if (result != 0) {
      fail("ReqSettlementInfoConfirm rejected rc=" + std::to_string(result));
    }
  }

  void onRspSettlementInfoConfirm(const CThostFtdcRspInfoField* info, bool isLast) {
    if (hasError(info)) {
      fail("settlement confirm failed: " + errorText(info));
      return;
    }
    if (!isLast) {
      return;
    }
    // queryInstruments() records its own failure reason.
    (void)queryInstruments();
  }

  void onRspQryInstrument(const CThostFtdcInstrumentField* row,
                          const CThostFtdcRspInfoField* info, bool isLast) {
    if (hasError(info)) {
      fail("instrument query failed: " + errorText(info));
      return;
    }
    if (row != nullptr) {
      Instrument instrument;
      instrument.instrumentId = row->InstrumentID;
      instrument.exchangeId = row->ExchangeID;
      instrument.productId = row->ProductID;
      instrument.volumeMultiple = row->VolumeMultiple;
      instrument.priceTick = row->PriceTick;
      std::lock_guard<std::mutex> lock(mutex_);
      instruments_.push_back(std::move(instrument));
    }
    if (isLast) {
      setState(SessionState::Ready);
      cv_.notify_all();
    }
  }

  void onRspError(const CThostFtdcRspInfoField* info, int requestId) {
    const std::string message =
        "rsp error request=" + std::to_string(requestId) + " " + errorText(info);
    bool ready = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ready = state_ == SessionState::Ready;
    }
    if (ready) {
      std::cerr << "[ctp] " << message << '\n';
      return;
    }
    fail(message);
  }

  void fail(const std::string& message) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_ || state_ == SessionState::Failed) {
        return;
      }
      state_ = SessionState::Failed;
      lastError_ = message;
    }
    std::cerr << "[ctp] failed: " << message << '\n';
    cv_.notify_all();
  }

  void setState(SessionState next) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      state_ = next;
    }
    logState();
  }

  CThostFtdcTraderApi* apiLocked() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return api_;
  }

  void logState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "[ctp] state=" << toString(state_) << '\n';
  }

  struct TraderSpi final : CThostFtdcTraderSpi {
    explicit TraderSpi(Impl& owner) : owner_(owner) {}

    void OnFrontConnected() override { owner_.onFrontConnected(); }

    void OnFrontDisconnected(int reason) override { owner_.onFrontDisconnected(reason); }

    void OnRspAuthenticate(CThostFtdcRspAuthenticateField*, CThostFtdcRspInfoField* info,
                           int, bool isLast) override {
      owner_.onRspAuthenticate(info, isLast);
    }

    void OnRspUserLogin(CThostFtdcRspUserLoginField* login, CThostFtdcRspInfoField* info,
                        int, bool isLast) override {
      owner_.onRspUserLogin(login, info, isLast);
    }

    void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField*,
                                    CThostFtdcRspInfoField* info, int, bool isLast) override {
      owner_.onRspSettlementInfoConfirm(info, isLast);
    }

    void OnRspQryInstrument(CThostFtdcInstrumentField* instrument,
                            CThostFtdcRspInfoField* info, int, bool isLast) override {
      owner_.onRspQryInstrument(instrument, info, isLast);
    }

    void OnRspError(CThostFtdcRspInfoField* info, int requestId, bool) override {
      owner_.onRspError(info, requestId);
    }

    Impl& owner_;
  };

  CtpConfig config_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  SessionState state_{SessionState::Disconnected};
  std::string lastError_;
  std::vector<Instrument> instruments_;
  std::atomic<int> requestId_{0};
  bool stopping_{false};
  CThostFtdcTraderApi* api_{nullptr};
  std::unique_ptr<TraderSpi> spi_;
};

CtpTraderSession::CtpTraderSession(CtpConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

CtpTraderSession::~CtpTraderSession() = default;

bool CtpTraderSession::start(std::chrono::milliseconds timeout) {
  return impl_->start(timeout);
}

SessionState CtpTraderSession::state() const {
  return impl_->state();
}

std::string CtpTraderSession::lastError() const {
  return impl_->lastError();
}

bool CtpTraderSession::queryInstruments() {
  return impl_->queryInstruments();
}

std::vector<Instrument> CtpTraderSession::instruments() const {
  return impl_->instruments();
}

}  // namespace ors::ctp

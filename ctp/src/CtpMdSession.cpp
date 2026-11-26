#include "ors/ctp/CtpSession.h"
#include "ors/ctp/TextEncoding.h"
#include "ors/ctp/PriceSanitize.h"

#include <ThostFtdcMdApi.h>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string_view>
#include <system_error>
#include <utility>

#include "ors/base/Log.h"

namespace ors::ctp {
namespace {

// The SPI helpers below mirror CtpTraderSession.cpp; both are a handful of
// lines and keeping them per-TU avoids touching the reviewed trader code.

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
  return std::to_string(info->ErrorID) + " " + gbkToUtf8(info->ErrorMsg);
}

std::string flowPathWithSlash(const std::string& path) {
  if (!path.empty() && path.back() != '/') {
    return path + "/";
  }
  return path;
}

DepthTick toDepthTick(const CThostFtdcDepthMarketDataField& row) {
  DepthTick tick;
  tick.instrumentId = row.InstrumentID;
  tick.exchangeId = row.ExchangeID;
  tick.lastPrice = row.LastPrice;
  tick.lastVolume = row.Volume;
  tick.turnover = row.Turnover;
  tick.openInterest = row.OpenInterest;
  tick.updateTime = row.UpdateTime;
  tick.updateMillisec = row.UpdateMillisec;
  tick.bidPrices = {row.BidPrice1, row.BidPrice2, row.BidPrice3, row.BidPrice4,
                    row.BidPrice5};
  tick.bidQtys = {row.BidVolume1, row.BidVolume2, row.BidVolume3, row.BidVolume4,
                  row.BidVolume5};
  tick.askPrices = {row.AskPrice1, row.AskPrice2, row.AskPrice3, row.AskPrice4,
                    row.AskPrice5};
  tick.askQtys = {row.AskVolume1, row.AskVolume2, row.AskVolume3, row.AskVolume4,
                  row.AskVolume5};
  return tick;
}

}  // namespace

MarketData mapDepthMarketData(const DepthTick& tick) {
  MarketData md;
  md.instrumentId = tick.instrumentId;
  md.exchange = tick.exchangeId;
  md.lastPrice = sanitizePrice(tick.lastPrice);
  md.volume = tick.lastVolume;
  md.turnover = sanitizeMoney(tick.turnover);
  md.openInterest = sanitizeMoney(tick.openInterest);
  md.updateTime = tick.updateTime;
  md.updateMillisec = tick.updateMillisec;
  for (std::size_t level = 0; level < kCtpDepthLevels; ++level) {
    md.bidPrices[level] = sanitizePrice(tick.bidPrices[level]);
    md.bidQtys[level] = tick.bidQtys[level];
    md.askPrices[level] = sanitizePrice(tick.askPrices[level]);
    md.askQtys[level] = tick.askQtys[level];
  }
  // Levels 6..20 keep the zero-initialised values: CTP has five levels, and
  // repeating level five would invent depth that does not exist.
  return md;
}

struct CtpMdSession::Impl {
  explicit Impl(CtpConfig config, std::shared_ptr<spdlog::logger> logger)
      : config_(std::move(config)), logger_(std::move(logger)) {}
  ~Impl() { stop(); }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void emit(spdlog::level::level_enum level, std::string_view event,
            std::initializer_list<base::LogField> fields) const {
    if (logger_) {
      base::logEvent(*logger_, level, event, fields);
    }
  }

  bool start(std::chrono::milliseconds timeout) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ != SessionState::Disconnected) {
        lastError_ = "md session already started";
        return false;
      }
      state_ = SessionState::Connecting;
    }
    logState();

    std::error_code error;
    std::filesystem::create_directories(config_.mdFlowPath, error);
    if (error) {
      fail("cannot create md flow path " + config_.mdFlowPath + ": " + error.message());
      return false;
    }

    CThostFtdcMdApi* api = CThostFtdcMdApi::CreateFtdcMdApi(
        flowPathWithSlash(config_.mdFlowPath).c_str(), false, false, true);
    if (api == nullptr) {
      fail("CreateFtdcMdApi returned null");
      return false;
    }
    spi_ = std::make_unique<MdSpi>(*this);
    api->RegisterSpi(spi_.get());
    api->RegisterFront(const_cast<char*>(config_.mdFront.c_str()));
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
          lastError_ = "md login timed out in state " + std::string(toString(state_));
          state_ = SessionState::Failed;
        }
        failure = lastError_;
      }
    }
    if (!ready) {
      if (logger_) {
        emit(spdlog::level::err, "session.handshake",
             {base::LogField("session", "md"),
              base::LogField("step", "login"),
              base::LogField("reason", failure)});
      } else {
        std::cerr << "[ctp] md login failed: " << failure << '\n';
      }
      stop();
    }
    return ready;
  }

  void stop() {
    CThostFtdcMdApi* api = nullptr;
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

  MarketData lastMarketData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastMarketData_;
  }

  std::size_t marketDataCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return marketDataCount_;
  }

  bool waitForMarketData(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, timeout, [this] { return marketDataCount_ > 0; });
    return marketDataCount_ > 0;
  }

  bool subscribe(const std::vector<std::string>& instruments) {
    if (instruments.empty()) {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = "subscribe: no instruments given";
      return false;
    }
    CThostFtdcMdApi* api = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_ = instruments;
      if (state_ != SessionState::Ready) {
        return true;  // sent from onRspUserLogin
      }
      api = api_;
    }
    return sendSubscribe(api, instruments);
  }

  bool sendSubscribe(CThostFtdcMdApi* api, const std::vector<std::string>& instruments) {
    if (api == nullptr) {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = "subscribe: md api is not initialized";
      return false;
    }
    // SubscribeMarketData wants mutable char*; CTP copies the ids.
    std::vector<std::string> ids = instruments;
    std::vector<char*> pointers;
    pointers.reserve(ids.size());
    for (std::string& id : ids) {
      pointers.push_back(id.data());
    }
    const int result =
        api->SubscribeMarketData(pointers.data(), static_cast<int>(pointers.size()));
    if (result != 0) {
      const std::string message = "SubscribeMarketData rejected rc=" +
                                  std::to_string(result);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = message;
      }
      if (logger_) {
        emit(spdlog::level::err, "md.subscribe_failed",
             {base::LogField("reason", message)});
      } else {
        std::cerr << "[ctp] " << message << '\n';
      }
      return false;
    }
    return true;
  }

  void onFrontConnected() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_ || state_ != SessionState::Connecting) {
        return;
      }
      state_ = SessionState::LoggingIn;
    }
    logState();

    CThostFtdcMdApi* api = apiLocked();
    if (api == nullptr) {
      return;
    }
    CThostFtdcReqUserLoginField request;
    std::memset(&request, 0, sizeof(request));
    copyField(request.BrokerID, config_.brokerId);
    copyField(request.UserID, config_.investorId);
    copyField(request.Password, config_.password);
    const int result = api->ReqUserLogin(&request, ++requestId_);
    if (result != 0) {
      fail("md ReqUserLogin rejected rc=" + std::to_string(result));
    }
  }

  void onFrontDisconnected(int reason) {
    const std::string message = "md front disconnected reason=" + std::to_string(reason);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        return;
      }
      state_ = state_ == SessionState::Ready ? SessionState::Disconnected
                                             : SessionState::Failed;
      lastError_ = message;
    }
    if (logger_) {
      emit(spdlog::level::warn, "session.disconnected",
           {base::LogField("session", "md"),
            base::LogField("reason", reason)});
    } else {
      std::cerr << "[ctp] " << message << '\n';
    }
    cv_.notify_all();
  }

  void onRspUserLogin(const CThostFtdcRspInfoField* info, bool isLast) {
    if (hasError(info)) {
      fail("md login failed: " + errorText(info));
      return;
    }
    if (!isLast) {
      return;
    }
    std::vector<std::string> pending;
    CThostFtdcMdApi* api = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      state_ = SessionState::Ready;
      pending = pending_;
      api = api_;
    }
    logState();
    cv_.notify_all();
    if (!pending.empty()) {
      (void)sendSubscribe(api, pending);
    }
  }

  void onRspSubMarketData(const CThostFtdcSpecificInstrumentField* instrument,
                          const CThostFtdcRspInfoField* info) {
    if (!hasError(info)) {
      return;
    }
    // One bad contract must not take the session down; the smoke run reports
    // it as "no market data" together with this text.
    const std::string message =
        "subscribe failed instrument=" +
        std::string(instrument != nullptr ? instrument->InstrumentID : "?") + " " +
        errorText(info);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lastError_ = message;
    }
    if (logger_) {
      emit(spdlog::level::warn, "md.subscribe_failed",
           {base::LogField("reason", message)});
    } else {
      std::cerr << "[ctp] " << message << '\n';
    }
  }

  void onRtnDepthMarketData(const CThostFtdcDepthMarketDataField& row) {
    MarketData data = mapDepthMarketData(toDepthTick(row));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lastMarketData_ = std::move(data);
      ++marketDataCount_;
    }
    cv_.notify_all();
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
      if (logger_) {
        emit(spdlog::level::warn, "session.error",
             {base::LogField("session", "md"),
              base::LogField("reason", message)});
      } else {
        std::cerr << "[ctp] " << message << '\n';
      }
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
    if (logger_) {
      emit(spdlog::level::err, "session.failed",
           {base::LogField("session", "md"),
            base::LogField("reason", message)});
    } else {
      std::cerr << "[ctp] failed: " << message << '\n';
    }
    cv_.notify_all();
  }

  CThostFtdcMdApi* apiLocked() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return api_;
  }

  void logState() const {
    std::string state;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      state = toString(state_);
    }
    if (logger_) {
      emit(spdlog::level::info, "session.state",
           {base::LogField("session", "md"),
            base::LogField("state", state)});
    } else {
      std::cerr << "[ctp] md state=" << state << '\n';
    }
  }

  struct MdSpi final : CThostFtdcMdSpi {
    explicit MdSpi(Impl& owner) : owner_(owner) {}

    void OnFrontConnected() override { owner_.onFrontConnected(); }

    void OnFrontDisconnected(int reason) override { owner_.onFrontDisconnected(reason); }

    void OnRspUserLogin(CThostFtdcRspUserLoginField*, CThostFtdcRspInfoField* info,
                        int, bool isLast) override {
      owner_.onRspUserLogin(info, isLast);
    }

    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField* instrument,
                            CThostFtdcRspInfoField* info, int, bool) override {
      owner_.onRspSubMarketData(instrument, info);
    }

    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* tick) override {
      if (tick != nullptr) {
        owner_.onRtnDepthMarketData(*tick);
      }
    }

    void OnRspError(CThostFtdcRspInfoField* info, int requestId, bool) override {
      owner_.onRspError(info, requestId);
    }

    Impl& owner_;
  };

  CtpConfig config_;
  std::shared_ptr<spdlog::logger> logger_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  SessionState state_{SessionState::Disconnected};
  std::string lastError_;
  std::vector<std::string> pending_;
  MarketData lastMarketData_;
  std::size_t marketDataCount_{0};
  std::atomic<int> requestId_{0};
  bool stopping_{false};
  CThostFtdcMdApi* api_{nullptr};
  std::unique_ptr<MdSpi> spi_;
};

CtpMdSession::CtpMdSession(CtpConfig config, std::shared_ptr<spdlog::logger> logger)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(logger))) {}

CtpMdSession::~CtpMdSession() = default;

bool CtpMdSession::start(std::chrono::milliseconds timeout) {
  return impl_->start(timeout);
}

SessionState CtpMdSession::state() const {
  return impl_->state();
}

std::string CtpMdSession::lastError() const {
  return impl_->lastError();
}

bool CtpMdSession::subscribe(const std::vector<std::string>& instruments) {
  return impl_->subscribe(instruments);
}

bool CtpMdSession::waitForMarketData(std::chrono::milliseconds timeout) {
  return impl_->waitForMarketData(timeout);
}

MarketData CtpMdSession::lastMarketData() const {
  return impl_->lastMarketData();
}

std::size_t CtpMdSession::marketDataCount() const {
  return impl_->marketDataCount();
}

}  // namespace ors::ctp

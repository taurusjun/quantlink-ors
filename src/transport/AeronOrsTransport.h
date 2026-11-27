#pragma once

#include <array>
#include <utility>
#include <functional>

#include "config/OrsConfig.h"
#include "transport/Frame.h"
#include "marketdata/MarketDataSink.h"
#include "transport/OrsTransport.h"

namespace ors {

class AeronOrsTransport final : public OrsTransport, public MarketDataSink {
 public:
  AeronOrsTransport(OrsConfig config, FramePublisher& publisher,
                    FrameSubscriber& subscriber);

  bool start(RequestHandler requestHandler,
             AccountQueryHandler accountQueryHandler,
             ClientRegisterHandler clientRegisterHandler) override;
  bool sendResponse(const ::ors::infra::ResponseMsg& response) override;
  bool sendMarketData(const ::ors::md::MarketUpdateNew& update) override;
  bool send(const ::ors::md::MarketUpdateNew& update) override {
    return sendMarketData(update);
  }
  bool sendSession(const SessionEvent& event) override;
  bool sendAccount(const AccountSnapshot& snapshot) override;
  bool sendClientRegisterResponse(const ClientRegisterResult& result) override;
  void stop() override;

  void poll(int fragmentLimit);

 private:
  bool publish(int streamId, std::size_t length);

  OrsConfig config_;
  FramePublisher& publisher_;
  FrameSubscriber& subscriber_;
  std::array<char, 65536> buffer_{};
  RequestHandler requestHandler_;
  AccountQueryHandler accountQueryHandler_;
  ClientRegisterHandler clientRegisterHandler_;
};

}  // namespace ors

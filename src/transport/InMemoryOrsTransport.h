#pragma once

#include <cstddef>
#include <vector>

#include "transport/OrsTransport.h"
#include "marketdata/MarketDataSink.h"

namespace ors {

class InMemoryOrsTransport final : public OrsTransport, public MarketDataSink {
 public:
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

  std::size_t responseCount() const { return responses_.size(); }
  const std::vector<::ors::infra::ResponseMsg>& responses() const {
    return responses_;
  }
  const std::vector<AccountSnapshot>& accounts() const { return accounts_; }
  const std::vector<SessionEvent>& sessions() const { return sessions_; }

  RequestHandler requestHandler;
  AccountQueryHandler accountQueryHandler;
  ClientRegisterHandler clientRegisterHandler;

 private:
  std::vector<::ors::infra::ResponseMsg> responses_;
  std::vector<::ors::md::MarketUpdateNew> marketData_;
  std::vector<SessionEvent> sessions_;
  std::vector<AccountSnapshot> accounts_;
  std::vector<ClientRegisterResult> clientRegistrations_;
};

}  // namespace ors

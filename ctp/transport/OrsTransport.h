#pragma once

#include <functional>

#include "core/OrsTypes.h"
#include "marketupdateNew.h"
#include "orderresponse.h"

namespace ors {

class OrsTransport {
 public:
  using RequestHandler =
      std::function<void(illuminati::infra::RequestMsg*)>;
  using AccountQueryHandler =
      std::function<void(const AccountQueryRequest&)>;
  using ClientRegisterHandler =
      std::function<void(const ClientRegisterRequest&)>;

  virtual ~OrsTransport() = default;

  virtual bool start(RequestHandler requestHandler,
                     AccountQueryHandler accountQueryHandler,
                     ClientRegisterHandler clientRegisterHandler) = 0;
  virtual bool sendResponse(const illuminati::infra::ResponseMsg& response) = 0;
  virtual bool sendMarketData(const illuminati::md::MarketUpdateNew& update) = 0;
  virtual bool sendSession(const SessionEvent& event) = 0;
  virtual bool sendAccount(const AccountSnapshot& snapshot) = 0;
  virtual bool sendClientRegisterResponse(const ClientRegisterResult& result) = 0;
  virtual void stop() = 0;
};

}  // namespace ors

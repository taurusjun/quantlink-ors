#include "transport/InMemoryOrsTransport.h"

#include <utility>

namespace ors {

bool InMemoryOrsTransport::start(RequestHandler requestHandler,
                                 AccountQueryHandler accountQueryHandler,
                                 ClientRegisterHandler clientRegisterHandler) {
  this->requestHandler = std::move(requestHandler);
  this->accountQueryHandler = std::move(accountQueryHandler);
  this->clientRegisterHandler = std::move(clientRegisterHandler);
  return true;
}

bool InMemoryOrsTransport::sendResponse(
    const illuminati::infra::ResponseMsg& response) {
  responses_.push_back(response);
  return true;
}

bool InMemoryOrsTransport::sendMarketData(
    const illuminati::md::MarketUpdateNew& update) {
  marketData_.push_back(update);
  return true;
}

bool InMemoryOrsTransport::sendSession(const SessionEvent& event) {
  sessions_.push_back(event);
  return true;
}

bool InMemoryOrsTransport::sendAccount(const AccountSnapshot& snapshot) {
  accounts_.push_back(snapshot);
  return true;
}

bool InMemoryOrsTransport::sendClientRegisterResponse(
    const ClientRegisterResult& result) {
  clientRegistrations_.push_back(result);
  return true;
}

void InMemoryOrsTransport::stop() {}

}  // namespace ors

#include <cstring>
#include <iostream>

#include "core/ORSServer.h"
#include "transport/InMemoryOrsTransport.h"

int main() {
  ::ors::OrsConfig config;
  config.accountId = "142266";
  config.venueId = "SIMNOW";
  config.orsId = "ors-test";

  ::ors::InMemoryOrsTransport transport;
  ORSServer server(config, transport);

  ::ors::ClientRegisterRequest registration;
  registration.requestId = 1;
  registration.instanceId = "test";
  registration.accountId = "142266";
  server.RegisterClient(registration);

  illuminati::infra::RequestMsg cancel{};
  cancel.Request_Type = illuminati::infra::CANCELORDER;
  cancel.OrderID = 999;
  std::strcpy(cancel.AccountID, "142266");
  std::strcpy(cancel.Contract_Description.Symbol, "au2612");
  server.ProcessRequest(&cancel);

  if (transport.responseCount() != 1) {
    std::cerr << "expected one response, got " << transport.responseCount() << '\n';
    return 1;
  }
  const auto responses = transport.responses();
  if (responses.front().Response_Type != illuminati::infra::CANCEL_ORDER_REJECT ||
      responses.front().OrderID != 999) {
    std::cerr << "unknown cancel did not return CANCEL_ORDER_REJECT\n";
    return 1;
  }
  return 0;
}

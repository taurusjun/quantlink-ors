#include <cstring>
#include <iostream>

#include "session/ORSServer.h"
#include "transport/InMemoryOrsTransport.h"

int main() {
  ::ors::OrsConfig config;
  config.accountId = "142266";
  config.venueId = "SIMNOW";
  config.orsId = "ors-test";

  ::ors::InMemoryOrsTransport transport;
  MDHandler mdHandler(transport);
  ::ors::md::MarketUpdateNew marketUpdate{};
  ORSServer server(config, transport, mdHandler, marketUpdate);

  // StartupQuery must be constructed before CTP delivers the position callback.
  server.OnRspQryInvestorPosition(nullptr, nullptr, 0, true);

  ::ors::ClientRegisterRequest registration;
  registration.requestId = 1;
  registration.instanceId = "test";
  registration.accountId = "142266";
  server.RegisterClient(registration);

  ::ors::infra::RequestMsg cancel{};
  cancel.Request_Type = ::ors::infra::CANCELORDER;
  cancel.OrderID = 999;
  std::strcpy(cancel.AccountID, "142266");
  std::strcpy(cancel.Contract_Description.Symbol, "au2612");
  server.ProcessRequest(&cancel);

  if (transport.responseCount() != 1) {
    std::cerr << "expected one response, got " << transport.responseCount() << '\n';
    return 1;
  }
  const auto responses = transport.responses();
  if (responses.front().Response_Type != ::ors::infra::CANCEL_ORDER_REJECT ||
      responses.front().OrderID != 999) {
    std::cerr << "unknown cancel did not return CANCEL_ORDER_REJECT\n";
    return 1;
  }

  ::ors::OrderInfo order{};
  order.localID = 42;
  order.requestType = ::ors::infra::CANCELORDER;
  order.Transaction_Type = 'B';
  order.Qty = 1;
  order.Price = 725.0;
  std::strcpy(order.exchID, "000000000001");
  std::strcpy(order.ScripCode, "au2612");
  server.orderStore().local()[42] = order;
  server.orderStore().exchange()["000000000001"] = 42;

  CThostFtdcOrderActionField action{};
  std::strcpy(action.OrderRef, "000000000001");
  action.LimitPrice = 725.0;
  action.VolumeChange = 1;
  CThostFtdcRspInfoField error{};
  error.ErrorID = 26;
  std::strcpy(error.ErrorMsg, "cancel reject");
  server.OnRspOrderAction(&action, &error, 1, true);

  if (transport.responseCount() != 2 ||
      transport.responses()[1].Response_Type !=
          ::ors::infra::CANCEL_ORDER_REJECT ||
      server.orderStore().local().count(42) != 1) {
    std::cerr << "cancel reject did not keep the original order\n";
    return 1;
  }

  CThostFtdcOrderField stale{};
  std::strcpy(stale.OrderRef, "000000000001");
  stale.FrontID = 99;
  stale.SessionID = 99;
  stale.OrderSubmitStatus = THOST_FTDC_OSS_Accepted;
  stale.OrderStatus = THOST_FTDC_OST_NoTradeQueueing;
  const std::size_t before = transport.responseCount();
  server.OnRtnOrder(&stale);
  if (transport.responseCount() != before) {
    std::cerr << "stale front/session report was not ignored\n";
    return 1;
  }

  CThostFtdcTradeField trade{};
  std::strcpy(trade.OrderRef, "000000000001");
  std::strcpy(trade.OrderSysID, "12345");
  trade.Volume = 1;
  trade.Price = 725.0;
  const std::size_t beforeTrade = transport.responseCount();
  server.OnRtnTrade(&trade);
  if (transport.responseCount() != beforeTrade + 1 ||
      transport.responses().back().Response_Type !=
          ::ors::infra::TRADE_CONFIRM) {
    std::cerr << "trade confirm was not emitted\n";
    return 1;
  }

  ::ors::infra::RequestMsg preRms{};
  preRms.Request_Type = ::ors::infra::NEWORDER;
  preRms.OrderID = 1001;
  preRms.Quantity = 1;
  preRms.Price = 725.0;
  std::strcpy(preRms.AccountID, "142266");
  std::strcpy(preRms.Contract_Description.Symbol, "au2612");
  const std::size_t beforePreRms = transport.responseCount();
  server.ProcessRequest(&preRms);
  if (transport.responseCount() != beforePreRms + 1 ||
      transport.responses().back().Response_Type != ::ors::infra::ORS_REJECT) {
    std::cerr << "order before RMS ready was not rejected\n";
    return 1;
  }

  ::ors::InMemoryOrsTransport blockedTransport;
  MDHandler blockedMd(blockedTransport);
  ::ors::md::MarketUpdateNew blockedMarket{};
  ORSServer blocked(config, blockedTransport, blockedMd, blockedMarket);
  CThostFtdcOrderField live{};
  std::strcpy(live.OrderRef, "000000000002");
  live.OrderStatus = THOST_FTDC_OST_NoTradeQueueing;
  blocked.OnRspQryOrder(&live, nullptr, 0, true);
  if (!blocked.startupBlocked()) {
    std::cerr << "live order did not block startup\n";
    return 1;
  }
  blocked.Start();
  return 0;
}

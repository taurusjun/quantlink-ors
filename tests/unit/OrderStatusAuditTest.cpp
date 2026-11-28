#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "domain/OrderInfo.h"
#include "infra/LogFacade.h"
#include "marketdata/MDHandler.h"
#include "session/ORSServer.h"
#include "transport/InMemoryOrsTransport.h"

namespace {

std::size_t count(const std::string& text, const std::string& needle) {
  std::size_t total = 0;
  std::size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::string::npos) {
    ++total;
    offset += needle.size();
  }
  return total;
}

}  // namespace

int main() {
  const std::string path = "/tmp/ors_order_status_audit_test.jsonl";
  std::remove(path.c_str());

  auto logger = ::ors::base::createLogger("order-status-audit", path);
  ::ors::base::LogContext context;
  context.service = "ors";
  context.instance = "ors-test";
  context.host = "test-host";
  context.pid = 12345;
  ::ors::log::init(logger, context);

  ::ors::OrsConfig config;
  config.accountId = "142266";
  config.venueId = "SIMNOW";
  config.orsId = "ors-test";
  ::ors::InMemoryOrsTransport transport;
  MDHandler mdHandler(transport);
  ::ors::md::MarketUpdateNew marketUpdate{};
  ORSServer server(config, transport, mdHandler, marketUpdate);

  ::ors::OrderInfo order{};
  order.localID = 42;
  order.requestType = ::ors::infra::NEWORDER;
  order.orderSent = 0;
  std::strcpy(order.exchID, "000000000001");
  std::strcpy(order.ClientID, "142266");
  std::strcpy(order.ScripCode, "au2612");
  order.Transaction_Type = 'B';
  order.Price = 778.5;
  order.Qty = 2;
  order.openQty = 2;
  order.frontId = 1;
  order.sessionId = 2;
  order.strategyId = 14526;
  order.exchange = 57;
  order.qtyType = 3;
  server.orderStore().local()[42] = order;
  server.orderStore().exchange()["000000000001"] = 42;

  CThostFtdcOrderField report{};
  std::strcpy(report.OrderRef, "000000000001");
  std::strcpy(report.OrderSysID, "EX-42");
  report.FrontID = 1;
  report.SessionID = 2;
  report.OrderSubmitStatus = THOST_FTDC_OSS_Accepted;
  report.OrderStatus = THOST_FTDC_OST_NoTradeQueueing;
  report.VolumeTotalOriginal = 2;
  report.VolumeTraded = 0;
  report.LimitPrice = 778.5;
  server.OnRtnOrder(&report);

  report.OrderStatus = THOST_FTDC_OST_PartTradedQueueing;
  report.VolumeTraded = 1;
  server.OnRtnOrder(&report);

  report.OrderStatus = THOST_FTDC_OST_AllTraded;
  report.VolumeTraded = 2;
  server.OnRtnOrder(&report);

  // Duplicate terminal report must not create another status event.
  server.OnRtnOrder(&report);

  CThostFtdcTradeField trade{};
  std::strcpy(trade.OrderRef, "000000000001");
  std::strcpy(trade.TradeID, "T-42-1");
  trade.Volume = 1;
  trade.Price = 778.5;
  server.OnRtnTrade(&trade);

  logger->flush();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  logger->sinks().front()->flush();

  std::ifstream input(path);
  std::string all;
  std::string line;
  while (std::getline(input, line)) {
    all += line;
    all += '\n';
  }
  std::remove(path.c_str());

  if (count(all, "\"event\":\"ors.order.status\"") != 3) {
    std::cerr << "expected 3 status events:\n" << all;
    return 1;
  }
  if (count(all, "\"status\":\"accepted\"") == 0 ||
      count(all, "\"status\":\"partially_filled\"") == 0 ||
      count(all, "\"status\":\"filled\"") == 0) {
    std::cerr << "missing normalized status:\n" << all;
    return 1;
  }
  if (all.find("\"raw_status_name\":\"AllTraded\"") == std::string::npos ||
      all.find("\"source\":\"ctp_order_report\"") == std::string::npos) {
    std::cerr << "missing raw/source status fields:\n" << all;
    return 1;
  }
  if (all.find("\"event\":\"ors.order.trade\"") == std::string::npos ||
      all.find("\"exchange_trade_id\":\"T-42-1\"") == std::string::npos) {
    std::cerr << "missing trade id:\n" << all;
    return 1;
  }
  if (count(all, "\"raw_status_name\":\"AllTraded\"") != 1) {
    std::cerr << "duplicate AllTraded status was logged:\n" << all;
    return 1;
  }
  return 0;
}

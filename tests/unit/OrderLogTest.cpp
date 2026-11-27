#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "execution/OrderLog.h"
#include "infra/LogFacade.h"

int main() {
  const std::string path = "/tmp/ors_order_log_test.jsonl";
  std::remove(path.c_str());

  auto logger = ::ors::base::createLogger("order-log-test", path);
  ::ors::base::LogContext context;
  context.service = "ors";
  context.instance = "ors-test";
  context.host = "test-host";
  context.pid = 12345;
  ::ors::log::init(logger, context);

  ::ors::OrderInfo order;
  order.localID = 2000000;
  std::strcpy(order.exchID, "000000000001");
  std::strcpy(order.OrderSysID, "123456");
  order.exchange = 57;  // CHINA_SHFE
  std::strcpy(order.ScripCode, "au2612");
  std::strcpy(order.ClientID, "142266");
  order.Transaction_Type = 'B';
  order.qtyType = 3;  // OPEN_ORDER
  order.Price = 785.7;
  order.Qty = 1;

  ::ors::logOrderEvent("ors.order.trade", "order filled", order, "filled",
                       ::ors::OrderLogLevel::Info, 0, 785.7, 1, 1, 0);
  logger->flush();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  logger->sinks().front()->flush();

  std::ifstream input(path);
  std::string line;
  std::getline(input, line);
  std::remove(path.c_str());

  const char* required[] = {
      "\"type\":\"order\"",
      "\"order_id\":\"2000000\"",
      "\"order_ref\":\"000000000001\"",
      "\"exchange_order_id\":\"123456\"",
      "\"exchange_id\":\"SHFE\"",
      "\"instrument\":\"au2612\"",
      "\"account_id\":\"142266\"",
      "\"side\":\"BUY\"",
      "\"offset\":\"OPEN\"",
      "\"status\":\"filled\"",
      "\"price\":785.7",
      "\"quantity\":1",
      "\"filled_quantity\":1",
      "\"remaining_quantity\":0",
      "\"message\":\"order filled\"",
  };
  for (const char* value : required) {
    if (line.find(value) == std::string::npos) {
      std::cerr << "missing " << value << ": " << line << '\n';
      return 1;
    }
  }
  if (line.find("|") != std::string::npos ||
      line.find("orderId=") != std::string::npos) {
    std::cerr << "message still contains legacy separators: " << line << '\n';
    return 1;
  }
  return 0;
}

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "infra/LogFacade.h"

int main() {
  const std::string path = "/tmp/ors_log_facade_test.jsonl";
  std::remove(path.c_str());

  auto logger = ::ors::base::createLogger("ors-test", path);
  ::ors::base::LogContext context;
  context.service = "ors";
  context.instance = "ors-test";
  context.host = "test-host";
  context.pid = 12345;
  ::ors::log::init(logger, context);
  ::ors::log::info("order.rtn", {{"order_id", "42"},
                               {"symbol", "au2612"},
                               {"password", "must-not-appear"}});
  logger->flush();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  logger->sinks().front()->flush();

  std::ifstream input(path);
  std::string line;
  std::getline(input, line);
  std::remove(path.c_str());

  if (line.find("\"schema_version\":\"1\"") == std::string::npos ||
      line.find("\"type\":\"order\"") == std::string::npos ||
      line.find("\"service\":\"ors\"") == std::string::npos ||
      line.find("\"instance\":\"ors-test\"") == std::string::npos ||
      line.find("\"host\":\"test-host\"") == std::string::npos ||
      line.find("\"pid\":12345") == std::string::npos) {
    std::cerr << "missing common fields: " << line << '\n';
    return 1;
  }
  if (line.find("\"ts\":\"") == std::string::npos ||
      line.find("+0800") != std::string::npos ||
      line.find("Z\"") == std::string::npos) {
    std::cerr << "timestamp is not UTC: " << line << '\n';
    return 1;
  }
  if (line.find("\"event\":\"order.rtn\"") == std::string::npos) {
    std::cerr << "missing event: " << line << '\n';
    return 1;
  }
  if (line.find("\"order_id\":\"42\"") == std::string::npos) {
    std::cerr << "order_id is not a string: " << line << '\n';
    return 1;
  }
  if (line.find("must-not-appear") != std::string::npos) {
    std::cerr << "password leaked: " << line << '\n';
    return 1;
  }
  return 0;
}

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "log/Log.h"

int main() {
  const std::string path = "/tmp/ors_log_facade_test.jsonl";
  std::remove(path.c_str());

  auto logger = ors::base::createLogger("ors-test", path);
  ors::log::init(logger);
  ors::log::info("order.rtn", {{"order_id", 42},
                               {"symbol", "au2612"},
                               {"password", "must-not-appear"}});
  logger->flush();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  logger->sinks().front()->flush();

  std::ifstream input(path);
  std::string line;
  std::getline(input, line);
  std::remove(path.c_str());

  if (line.find("\"event\":\"order.rtn\"") == std::string::npos) {
    std::cerr << "missing event: " << line << '\n';
    return 1;
  }
  if (line.find("\"order_id\":42") == std::string::npos) {
    std::cerr << "missing numeric field: " << line << '\n';
    return 1;
  }
  if (line.find("must-not-appear") != std::string::npos) {
    std::cerr << "password leaked: " << line << '\n';
    return 1;
  }
  return 0;
}

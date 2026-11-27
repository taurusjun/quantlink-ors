#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "config/OrsConfig.h"
#include "domain/PositionTable.h"
#include "infra/LogFacade.h"
#include "query/StartupQuery.h"

int main() {
  const std::string path = "/tmp/ors_startup_query_test.jsonl";
  std::remove(path.c_str());

  auto logger = ::ors::base::createLogger("startup-query-test", path);
  ::ors::base::LogContext context;
  context.service = "ors";
  context.instance = "ors-test";
  context.host = "test-host";
  context.pid = 12345;
  ::ors::log::init(logger, context);

  ::ors::OrsConfig config;
  config.accountId = "142266";
  config.brokerId = "9999";
  config.investorId = "142266";
  config.venueId = "SIMNOW";
  config.orsId = "ors-test";

  ::ors::PositionTable positions;
  int positionRequests = 0;
  int orderRequests = 0;
  int readyCalls = 0;
  ::ors::StartupQuery query(
      positions, config,
      [&](CThostFtdcQryInvestorPositionField&) {
        ++positionRequests;
        return 0;
      },
      [&](CThostFtdcQryOrderField&) {
        ++orderRequests;
        return 0;
      },
      [&] { ++readyCalls; });

  query.requestPosition();
  query.onPosition(nullptr, nullptr, true);
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

  if (positionRequests != 1 || orderRequests != 1 || readyCalls != 1) {
    std::cerr << "unexpected callbacks: position=" << positionRequests
              << " order=" << orderRequests << " ready=" << readyCalls << '\n';
    return 1;
  }
  if (all.find("position_query_failed") != std::string::npos ||
      all.find("order_query_failed") != std::string::npos) {
    std::cerr << "successful CTP query was logged as failure:\n" << all;
    return 1;
  }
  return 0;
}

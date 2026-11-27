#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "config/OrsConfig.h"
#include "domain/PositionTable.h"
#include "infra/LogFacade.h"
#include "query/StartupQuery.h"

namespace {

struct Scenario {
  ::ors::PositionTable positions;
  int positionRequests{0};
  int orderRequests{0};
  int readyCalls{0};
  int blockedCalls{0};
  std::string blockedReason;
};

std::unique_ptr<::ors::StartupQuery> makeQuery(Scenario& scenario,
                                               int orderResult = 0) {
  return std::make_unique<::ors::StartupQuery>(
      scenario.positions, ::ors::OrsConfig{},
      [&](CThostFtdcQryInvestorPositionField&) {
        ++scenario.positionRequests;
        return 0;
      },
      [&, orderResult](CThostFtdcQryOrderField&) {
        ++scenario.orderRequests;
        return orderResult;
      },
      [&] { ++scenario.readyCalls; },
      [&](std::string_view reason) {
        ++scenario.blockedCalls;
        scenario.blockedReason = std::string(reason);
      });
}

void runPositionPhase(::ors::StartupQuery& query) {
  query.requestPosition();
  query.onPosition(nullptr, nullptr, true);
}

}  // namespace

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

  Scenario flat;
  auto flatQuery = makeQuery(flat);
  runPositionPhase(*flatQuery);
  flatQuery->onOrders(nullptr, nullptr, true);

  Scenario live;
  auto liveQuery = makeQuery(live);
  runPositionPhase(*liveQuery);
  CThostFtdcOrderField order{};
  std::strcpy(order.OrderRef, "000000000001");
  order.OrderStatus = THOST_FTDC_OST_NoTradeQueueing;
  liveQuery->onOrders(&order, nullptr, true);

  Scenario failed;
  auto failedQuery = makeQuery(failed, -1);
  failedQuery->requestPosition();
  failedQuery->onPosition(nullptr, nullptr, true);

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

  if (flat.positionRequests != 1 || flat.orderRequests != 1 ||
      flat.readyCalls != 1 || flat.blockedCalls != 0) {
    std::cerr << "flat callbacks: position=" << flat.positionRequests
              << " order=" << flat.orderRequests << " ready=" << flat.readyCalls
              << " blocked=" << flat.blockedCalls << '\n';
    return 1;
  }
  if (live.readyCalls != 0 || live.blockedCalls != 1 ||
      live.blockedReason.find("live orders") == std::string::npos) {
    std::cerr << "live callbacks: ready=" << live.readyCalls
              << " blocked=" << live.blockedCalls
              << " reason=" << live.blockedReason << '\n';
    return 1;
  }
  if (failed.readyCalls != 0 || failed.blockedCalls != 1) {
    std::cerr << "failed callbacks: ready=" << failed.readyCalls
              << " blocked=" << failed.blockedCalls << '\n';
    return 1;
  }
  if (all.find("ors.startup.live_orders") == std::string::npos ||
      all.find("\"restart_blocked\":true") == std::string::npos) {
    std::cerr << "missing live-order P1 log:\n" << all;
    return 1;
  }
  if (all.find("position_query_failed") != std::string::npos &&
      all.find("ors.startup.position_query_failed") != std::string::npos) {
    std::cerr << "unexpected position query failure:\n" << all;
    return 1;
  }
  return 0;
}

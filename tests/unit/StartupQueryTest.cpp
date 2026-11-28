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
  CThostFtdcInvestorPositionField position{};
  std::strcpy(position.InstrumentID, "au2612");
  position.PosiDirection = THOST_FTDC_PD_Long;
  position.PositionDate = THOST_FTDC_PSD_History;
  position.Position = 5;
  position.TodayPosition = 0;
  position.YdPosition = 5;
  position.HedgeFlag = THOST_FTDC_HF_Speculation;
  std::strcpy(position.ExchangeID, "SHFE");
  std::strcpy(position.TradingDay, "20261128");

  query.requestPosition();
  query.onPosition(&position, nullptr, false);
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

  const auto flatPosition = flat.positions.get("au2612");
  if (flatPosition.onLongPos != 5 || flatPosition.todayLongPos != 0) {
    std::cerr << "flat position table: on=" << flatPosition.onLongPos
              << " today=" << flatPosition.todayLongPos << '\n';
    return 1;
  }

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
  if (all.find("ors.startup.position_row") == std::string::npos ||
      all.find("\"instrument\":\"au2612\"") == std::string::npos ||
      all.find("\"side\":\"long\"") == std::string::npos ||
      all.find("\"position_date\":\"history\"") == std::string::npos ||
      all.find("\"position_date_code\":\"2\"") == std::string::npos ||
      all.find("\"position\":5") == std::string::npos ||
      all.find("\"today_position\":0") == std::string::npos ||
      all.find("\"yd_position\":5") == std::string::npos ||
      all.find("\"hedge_flag\":\"1\"") == std::string::npos ||
      all.find("\"exchange\":\"SHFE\"") == std::string::npos ||
      all.find("\"trading_day\":\"20261128\"") == std::string::npos) {
    std::cerr << "missing startup position data log:\n" << all;
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

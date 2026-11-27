#include "query/StartupQuery.h"

#include <cstring>
#include <string>

#include "execution/OpenOrderGuard.h"
#include "infra/LogFacade.h"

namespace ors {

StartupQuery::StartupQuery(PositionTable& positions, OrsConfig config,
                           PositionRequest positionRequest,
                           OrderRequest orderRequest, ReadyCallback ready)
    : positions_(positions), config_(std::move(config)),
      positionRequest_(std::move(positionRequest)),
      orderRequest_(std::move(orderRequest)), ready_(std::move(ready)) {}

void StartupQuery::requestPosition() {
  CThostFtdcQryInvestorPositionField request;
  std::memset(&request, 0, sizeof(request));
  std::strcpy(request.BrokerID, config_.brokerId.c_str());
  std::strcpy(request.InvestorID, config_.investorId.c_str());
  const int result = positionRequest_(request);
  if (result != 0) {
    log::error("ors.startup.position_query_failed",
               {{"type", "system"},
                {"account_id", config_.accountId},
                {"severity", "P1"},
                {"error_code", std::to_string(result)},
                {"message", "position query request failed"}});
  }
}

void StartupQuery::onPosition(CThostFtdcInvestorPositionField* position,
                              CThostFtdcRspInfoField* info, bool isLast) {
  if (info != nullptr && info->ErrorID != 0) {
    pendingPositions_.clear();
    log::error("ors.startup.position_query_failed",
               {{"type", "system"},
                {"account_id", config_.accountId},
                {"severity", "P1"},
                {"error_code", std::to_string(info->ErrorID)},
                {"error_message", info->ErrorMsg},
                {"message", "position query failed"}});
    return;
  }
  if (position != nullptr) {
    PositionRow row;
    row.instrumentId = position->InstrumentID;
    row.side = position->PosiDirection == THOST_FTDC_PD_Short
                   ? PositionSide::Short
                   : PositionSide::Long;
    row.position = position->Position;
    row.todayPosition = position->TodayPosition;
    pendingPositions_.push_back(std::move(row));
  }
  if (!isLast) {
    return;
  }

  positions_.replaceFromQuery(std::move(pendingPositions_));
  pendingPositions_.clear();
  if (ready_) {
    ready_();
  }
  log::info("ors.startup.position_ready",
            {{"type", "system"},
             {"account_id", config_.accountId},
             {"message", "ORS ready"}});
  requestOrders();
}

void StartupQuery::requestOrders() {
  if (orderQueryIssued_) {
    return;
  }
  orderQueryIssued_ = true;
  CThostFtdcQryOrderField request;
  std::memset(&request, 0, sizeof(request));
  std::strcpy(request.BrokerID, config_.brokerId.c_str());
  std::strcpy(request.InvestorID, config_.investorId.c_str());
  const int result = orderRequest_(request);
  if (result != 0) {
    log::error("ors.startup.order_query_failed",
               {{"type", "system"},
                {"account_id", config_.accountId},
                {"severity", "P1"},
                {"error_code", std::to_string(result)},
                {"message", "order query request failed"}});
  }
}

void StartupQuery::onOrders(CThostFtdcOrderField* order,
                            CThostFtdcRspInfoField* info, bool isLast) {
  if (info != nullptr && info->ErrorID != 0) {
    pendingOrders_.clear();
    log::error("ors.startup.order_query_failed",
               {{"type", "system"},
                {"account_id", config_.accountId},
                {"severity", "P1"},
                {"error_code", std::to_string(info->ErrorID)},
                {"error_message", info->ErrorMsg},
                {"message", "order query failed"}});
    return;
  }
  if (order != nullptr) {
    pendingOrders_.push_back(*order);
  }
  if (!isLast) {
    return;
  }

  if (hasLiveOrders(pendingOrders_)) {
    std::string refs;
    for (const auto& row : pendingOrders_) {
      if (row.OrderStatus == THOST_FTDC_OST_AllTraded ||
          row.OrderStatus == THOST_FTDC_OST_Canceled) {
        continue;
      }
      if (!refs.empty()) refs += ',';
      refs += row.OrderRef;
      if (refs.size() >= 512) break;
    }
    log::error("ors.startup.live_orders",
               {{"type", "system"},
                {"account_id", config_.accountId},
                {"severity", "P1"},
                {"message", "CTP has live orders at startup"},
                {"count", static_cast<int>(pendingOrders_.size())},
                {"order_refs", refs}});
  } else {
    log::info("ors.startup.orders_flat",
              {{"type", "system"},
               {"account_id", config_.accountId},
               {"message", "no live CTP orders at startup"}});
  }
  pendingOrders_.clear();
}

}  // namespace ors

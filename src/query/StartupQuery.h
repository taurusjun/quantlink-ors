#pragma once

#include <functional>
#include <vector>

#include <ThostFtdcUserApiStruct.h>

#include "config/OrsConfig.h"
#include "domain/PositionTable.h"

namespace ors {

/// Owns startup position/order queries and the live-order startup alarm.
/// It does not touch the CTP API directly; request adapters are injected.
class StartupQuery {
 public:
  using PositionRequest =
      std::function<int(CThostFtdcQryInvestorPositionField& request)>;
  using OrderRequest =
      std::function<int(CThostFtdcQryOrderField& request)>;
  using ReadyCallback = std::function<void()>;

  StartupQuery(PositionTable& positions, OrsConfig config,
               PositionRequest positionRequest, OrderRequest orderRequest,
               ReadyCallback ready);

  void requestPosition();
  void onPosition(CThostFtdcInvestorPositionField* position,
                  CThostFtdcRspInfoField* info, bool isLast);

  void onOrders(CThostFtdcOrderField* order, CThostFtdcRspInfoField* info,
                bool isLast);

 private:
  void requestOrders();

  PositionTable& positions_;
  OrsConfig config_;
  PositionRequest positionRequest_;
  OrderRequest orderRequest_;
  ReadyCallback ready_;
  std::vector<PositionRow> pendingPositions_;
  std::vector<CThostFtdcOrderField> pendingOrders_;
  bool orderQueryIssued_{false};
};

}  // namespace ors

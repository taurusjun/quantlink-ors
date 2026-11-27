#pragma once

#include <functional>
#include <string_view>
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
  using BlockedCallback = std::function<void(std::string_view reason)>;

  StartupQuery(PositionTable& positions, OrsConfig config,
               PositionRequest positionRequest, OrderRequest orderRequest,
               ReadyCallback ready, BlockedCallback blocked);

  void requestPosition();
  void onPosition(CThostFtdcInvestorPositionField* position,
                  CThostFtdcRspInfoField* info, bool isLast);

  void onOrders(CThostFtdcOrderField* order, CThostFtdcRspInfoField* info,
                bool isLast);

 private:
  void requestOrders();
  void block(std::string_view reason);

  PositionTable& positions_;
  OrsConfig config_;
  PositionRequest positionRequest_;
  OrderRequest orderRequest_;
  ReadyCallback ready_;
  BlockedCallback blocked_;
  std::vector<PositionRow> pendingPositions_;
  std::vector<CThostFtdcOrderField> pendingOrders_;
  bool orderQueryIssued_{false};
};

}  // namespace ors

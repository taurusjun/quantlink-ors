#include "execution/OrderService.h"

#include <cstring>

#include "infra/LogFacade.h"
#include "infra/Time.h"

namespace ors {

OrderService::OrderService(OrderStore& orders, PushResponse push)
    : orders_(orders), push_(std::move(push)) {}

bool OrderService::crossCheck(const ::ors::infra::RequestMsg& request) {
  std::lock_guard<std::mutex> lock(orders_.localMutex());
  for (const auto& entry : orders_.local()) {
    const OrderInfo& open = entry.second;
    if (request.Transaction_Type == open.Transaction_Type) {
      continue;
    }
    if (((open.Transaction_Type == 'B' && request.Price <= open.Price) ||
         (open.Transaction_Type == 'S' &&
          (request.Price >= open.Price || request.Price == 0))) &&
        std::strcmp(request.Contract_Description.Symbol, open.ScripCode) == 0) {
      log::info("ors.order.cross_reject",
                {{"open_order_id", static_cast<std::uint64_t>(entry.first)},
                 {"new_order_id", static_cast<std::uint64_t>(request.OrderID)}});
      return true;
    }
  }
  return false;
}

void OrderService::reject(const ::ors::infra::RequestMsg& request,
                          const char* reason) {
  ::ors::infra::ResponseMsg response{};
  if (request.Request_Type == ::ors::infra::NEWORDER) {
    response.Response_Type = ::ors::infra::ORS_REJECT;
    response.ErrorCode = kCrossRejectCode;
  } else if (request.Request_Type == ::ors::infra::MODIFYORDER) {
    response.Response_Type = ::ors::infra::MODIFY_ORDER_REJECT;
  } else if (request.Request_Type == ::ors::infra::CANCELORDER) {
    response.Response_Type = ::ors::infra::CANCEL_ORDER_REJECT;
  }
  response.OrderID = request.OrderID;
  std::strncpy(response.AccountID, request.AccountID,
               sizeof(response.AccountID) - 1);
  log::warn("ors.order.rejected",
            {{"order_id", static_cast<std::uint64_t>(request.OrderID)},
             {"reason", reason == nullptr ? "" : reason}});
  if (push_) {
    push_(&response);
  }
}

}  // namespace ors

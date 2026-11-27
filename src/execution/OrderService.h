#pragma once

#include <functional>
#include <string>

#include "domain/OrderInfo.h"
#include "execution/OrderStore.h"
#include "orderresponse.h"

namespace ors {

/// Order-side policy that does not own CTP or transport. It owns cross-order
/// checks and rejection responses over the shared OrderStore.
class OrderService {
 public:
  using PushResponse = std::function<void(::ors::infra::ResponseMsg*)>;

  OrderService(OrderStore& orders, PushResponse push);

  bool crossCheck(const ::ors::infra::RequestMsg& request);
  void reject(const ::ors::infra::RequestMsg& request, const char* reason);
  void setNextOrderRef(int value);
  std::string takeOrderRef();

  static constexpr std::uint32_t kCrossRejectCode = 1000101;

 private:
  OrderStore& orders_;
  PushResponse push_;
  int nextOrderRef_{0};
};

}  // namespace ors

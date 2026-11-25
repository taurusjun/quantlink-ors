#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Aeron.h"
#include "ors/ctp/CtpAdapter.h"

namespace ors::ctp {

/// Decodes contract OrderRequest/CancelRequest frames from the L1 command
/// streams and hands the resulting CTP-facing structs to the ORS runtime.
class AeronCommandSubscriber {
 public:
  using OrderHandler = std::function<void(const OrderRequest&)>;
  using CancelHandler = std::function<void(const CancelRequest&)>;

  AeronCommandSubscriber(std::string aeronDir, std::string channel,
                         int orderStream, int cancelStream);
  ~AeronCommandSubscriber();

  AeronCommandSubscriber(const AeronCommandSubscriber&) = delete;
  AeronCommandSubscriber& operator=(const AeronCommandSubscriber&) = delete;

  void poll(int fragmentLimit, const OrderHandler& onOrder,
            const CancelHandler& onCancel);

 private:
  std::shared_ptr<aeron::Aeron> aeron_;
  std::shared_ptr<aeron::Subscription> orderSubscription_;
  std::shared_ptr<aeron::Subscription> cancelSubscription_;
};

}  // namespace ors::ctp

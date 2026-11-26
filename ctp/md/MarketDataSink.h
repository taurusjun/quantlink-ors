#pragma once

#include "marketupdateNew.h"

namespace ors {

class MarketDataSink {
 public:
  virtual ~MarketDataSink() = default;
  virtual bool send(const ::ors::md::MarketUpdateNew& update) = 0;
};

}  // namespace ors

#pragma once

#include <mutex>

#include "domain/OrderInfo.h"

namespace ors {

/// Owns the two company ORS order maps and their locks. It deliberately
/// exposes the maps for the existing order state machine while keeping the
/// storage out of the CTP session class.
class OrderStore {
 public:
  OrderAccountMap& local() { return local_; }
  const OrderAccountMap& local() const { return local_; }
  OrderAccountMap2& exchange() { return exchange_; }
  const OrderAccountMap2& exchange() const { return exchange_; }
  std::mutex& localMutex() { return localMutex_; }
  std::mutex& exchangeMutex() { return exchangeMutex_; }

 private:
  OrderAccountMap local_;
  OrderAccountMap2 exchange_;
  std::mutex localMutex_;
  std::mutex exchangeMutex_;
};

}  // namespace ors

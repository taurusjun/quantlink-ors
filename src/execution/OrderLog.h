#pragma once

#include <cstdint>

#include "domain/OrderInfo.h"

namespace ors {

enum class OrderLogLevel { Info, Warn, Error };

/// Emits one structured order-lifecycle event. This helper only formats
/// fields; it never changes order state or control flow.
void logOrderEvent(const char* event, const char* message,
                   const OrderInfo& order, const char* status,
                   OrderLogLevel level = OrderLogLevel::Info,
                   std::int64_t errorCode = 0, double price = 0.0,
                   std::uint64_t quantity = 0,
                   std::uint64_t filledQuantity = 0,
                   std::uint64_t remainingQuantity = 0,
                   const char* errorMessage = nullptr,
                   const char* source = nullptr,
                   const char* rawStatus = nullptr,
                   const char* rawStatusName = nullptr,
                   const char* exchangeTradeId = nullptr);

}  // namespace ors

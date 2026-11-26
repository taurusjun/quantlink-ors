#pragma once

#include <vector>

#include <ThostFtdcUserApiStruct.h>

namespace ors {

/// A live order is any order CTP has not finished or canceled. A query failure
/// is handled by the caller as a blocked startup, not by this predicate.
bool hasLiveOrders(const std::vector<CThostFtdcOrderField>& orders);

}  // namespace ors

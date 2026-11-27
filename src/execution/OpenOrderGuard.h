#pragma once

#include <vector>

#include <ThostFtdcUserApiStruct.h>

namespace ors {

/// A live order is any order CTP has not finished or canceled. A query failure
/// is logged by the caller; startup is not blocked.
bool hasLiveOrders(const std::vector<CThostFtdcOrderField>& orders);

}  // namespace ors

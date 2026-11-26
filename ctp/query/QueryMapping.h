#pragma once

#include <cstdint>

#include <ThostFtdcUserApiStruct.h>

#include "core/OrsConfig.h"
#include "core/OrsTypes.h"

namespace ors {

AccountSnapshot mapAccountSnapshot(const CThostFtdcTradingAccountField& row,
                                   std::uint64_t requestId,
                                   const OrsConfig& config);

}  // namespace ors

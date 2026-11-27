#pragma once

#include <cstdint>

#include <ThostFtdcUserApiStruct.h>

#include "config/OrsConfig.h"
#include "domain/OrsTypes.h"

namespace ors {

AccountSnapshot mapAccountSnapshot(const CThostFtdcTradingAccountField& row,
                                   std::uint64_t requestId,
                                   const OrsConfig& config);

}  // namespace ors

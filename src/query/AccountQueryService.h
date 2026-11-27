#pragma once

#include <functional>
#include <cstdint>

#include <ThostFtdcUserApiStruct.h>

#include "config/OrsConfig.h"
#include "domain/OrsTypes.h"
#include "query/AccountQueryState.h"
#include "transport/OrsTransport.h"

namespace ors {

/// Owns account-query throttling, CTP request correlation and response/error
/// publication. The CTP request adapter is injected so this class is testable.
class AccountQueryService {
 public:
  using RequestFn =
      std::function<int(CThostFtdcQryTradingAccountField& request)>;

  AccountQueryService(OrsConfig config, OrsTransport& transport,
                      RequestFn request);

  void query(const AccountQueryRequest& request);
  void onResponse(CThostFtdcTradingAccountField* account,
                  CThostFtdcRspInfoField* info, int requestId, bool isLast);
  void expire();

 private:
  void sendError(std::uint64_t requestId, std::uint32_t code,
                 const char* message);

  OrsConfig config_;
  OrsTransport& transport_;
  RequestFn request_;
  AccountQueryState state_;
};

}  // namespace ors

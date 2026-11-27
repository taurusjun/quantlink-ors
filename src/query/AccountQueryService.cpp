#include "query/AccountQueryService.h"

#include <cstring>

#include "infra/Time.h"
#include "query/QueryMapping.h"

namespace ors {

AccountQueryService::AccountQueryService(OrsConfig config,
                                         OrsTransport& transport,
                                         RequestFn request)
    : config_(std::move(config)), transport_(transport),
      request_(std::move(request)) {}

void AccountQueryService::query(const AccountQueryRequest& request) {
  if (!state_.tryBegin(request.requestId, std::chrono::steady_clock::now(),
                       std::chrono::milliseconds(config_.accountQueryTimeoutMs))) {
    sendError(request.requestId, 3, "account query busy or throttled");
    return;
  }

  CThostFtdcQryTradingAccountField query;
  std::memset(&query, 0, sizeof(query));
  std::strcpy(query.BrokerID, config_.brokerId.c_str());
  std::strcpy(query.InvestorID, config_.investorId.c_str());
  const int ctpRequestId = request_(query);
  if (ctpRequestId <= 0) {
    state_.finish();
    sendError(request.requestId, 2, "CTP account query rejected");
    return;
  }
  state_.bindCtpRequest(ctpRequestId);
}

void AccountQueryService::onResponse(CThostFtdcTradingAccountField* account,
                                     CThostFtdcRspInfoField* info,
                                     int requestId, bool isLast) {
  const auto sdkRequestId = state_.requestIdFor(requestId);
  if (!sdkRequestId.has_value()) {
    return;
  }
  if (info != nullptr && info->ErrorID != 0) {
    sendError(*sdkRequestId, static_cast<std::uint32_t>(info->ErrorID),
              info->ErrorMsg);
    state_.finish();
    return;
  }
  if (!isLast) {
    return;
  }
  if (account == nullptr) {
    sendError(*sdkRequestId, 5, "empty account query response");
    state_.finish();
    return;
  }

  AccountSnapshot snapshot = mapAccountSnapshot(*account, *sdkRequestId, config_);
  snapshot.timestamp = nowNanos();
  transport_.sendAccount(snapshot);
  state_.finish();
}

void AccountQueryService::expire() {
  const auto expired = state_.expire(std::chrono::steady_clock::now());
  if (!expired.has_value()) {
    return;
  }
  AccountSnapshot snapshot;
  snapshot.requestId = *expired;
  snapshot.accountId = config_.accountId;
  snapshot.errorCode = 4;
  snapshot.errorMsg = "account query timeout";
  snapshot.timestamp = nowNanos();
  transport_.sendAccount(snapshot);
}

void AccountQueryService::sendError(std::uint64_t requestId,
                                    std::uint32_t code,
                                    const char* message) {
  AccountSnapshot snapshot;
  snapshot.requestId = requestId;
  snapshot.accountId = config_.accountId;
  snapshot.errorCode = code;
  snapshot.errorMsg = message == nullptr ? "" : message;
  snapshot.timestamp = nowNanos();
  transport_.sendAccount(snapshot);
}

}  // namespace ors

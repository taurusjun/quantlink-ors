#include <ThostFtdcMdApi.h>
#include <ThostFtdcTraderApi.h>

#if defined(__APPLE__)
// The macOS CTP framework in the local test staging does not export the Linux
// factory signatures. This stub is only for link tests; Linux uses the SDK.
CThostFtdcTraderApi *CThostFtdcTraderApi::CreateFtdcTraderApi(const char *,
                                                             bool) {
  return nullptr;
}
CThostFtdcMdApi *CThostFtdcMdApi::CreateFtdcMdApi(const char *, const bool,
                                                 const bool, bool) {
  return nullptr;
}
#endif

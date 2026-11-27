#include "session/ORSServer.h"
#include "infra/LogFacade.h"
#include "infra/Metrics.h"
#include "infra/Time.h"

#include <cstdlib>

#include "rmsimpl.hpp"

void ORSServer::OnRspAuthenticate(CThostFtdcRspAuthenticateField *,
                                  CThostFtdcRspInfoField *pRspInfo, int,
                                  bool) {
  if (pRspInfo != nullptr && pRspInfo->ErrorID != 0) {
    return;
  }
  ReqUserLogin();
}

void ORSServer::OnFrontDisconnected(int nReason) {
  m_connected = false;
  ORS_LOG(INFO) << "Front disconnected: " << nReason << std::endl;
}

void ORSServer::OnRspError(CThostFtdcRspInfoField *pRspInfo, int, bool) {
  IsErrorRspInfo(pRspInfo);
}

void ORSServer::OnHeartBeatWarning(int nTimeLapse) {
  ORS_LOG(INFO) << "Heartbeat warning: " << nTimeLapse << std::endl;
}

bool ORSServer::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo) {
  const bool failed = pRspInfo != nullptr && pRspInfo->ErrorID != 0;
  if (failed) {
    ORS_LOG(INFO) << "CTP error " << pRspInfo->ErrorID << ": "
               << pRspInfo->ErrorMsg << std::endl;
  }
  return failed;
}

void ORSServer::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
                               CThostFtdcRspInfoField *pRspInfo, int,
                               bool bIsLast) {
  if (!bIsLast || IsErrorRspInfo(pRspInfo)) {
    ORS_LOG(INFO) << "Trader login failed" << std::endl;
    return;
  }

  iNextOrderRef = std::atoi(pRspUserLogin->MaxOrderRef);
  FRONT_ID = pRspUserLogin->FrontID;
  SESSION_ID = pRspUserLogin->SessionID;

  if (m_pUserMDSpi != nullptr &&
      !m_pUserMDSpi->waitForLogin(std::chrono::seconds(3))) {
    ORS_LOG(ERROR) << "MD login timeout; ORS startup aborted" << std::endl;
    return;
  }

  ReqSettlementInfoConfirm();
  rms_.impl().initRmsImpl(m_maxClients, m_rmsConfigFile, m_rmsFirmwideConfigFile, "");
  rmsReady_ = true;
  m_logonResp = true;
}

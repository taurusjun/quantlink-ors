#include "ORSServer.h"

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
  ILOG(INFO) << "Front disconnected: " << nReason << std::endl;
}

void ORSServer::OnRspError(CThostFtdcRspInfoField *pRspInfo, int, bool) {
  IsErrorRspInfo(pRspInfo);
}

void ORSServer::OnHeartBeatWarning(int nTimeLapse) {
  ILOG(INFO) << "Heartbeat warning: " << nTimeLapse << std::endl;
}

bool ORSServer::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo) {
  const bool failed = pRspInfo != nullptr && pRspInfo->ErrorID != 0;
  if (failed) {
    ILOG(INFO) << "CTP error " << pRspInfo->ErrorID << ": "
               << pRspInfo->ErrorMsg << std::endl;
  }
  return failed;
}

void ORSServer::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
                               CThostFtdcRspInfoField *pRspInfo, int,
                               bool bIsLast) {
  if (!bIsLast || IsErrorRspInfo(pRspInfo)) {
    ILOG(INFO) << "Trader login failed" << std::endl;
    return;
  }

  iNextOrderRef = std::atoi(pRspUserLogin->MaxOrderRef);
  FRONT_ID = pRspUserLogin->FrontID;
  SESSION_ID = pRspUserLogin->SessionID;

  while (m_pUserMDSpi != nullptr && m_pUserMDSpi->m_iLoginMD == 0) {
  }

  ReqSettlementInfoConfirm();
  rms.initRmsImpl(m_maxClients, m_rmsConfigFile, m_rmsFirmwideConfigFile, "");
  m_logonResp = true;
}

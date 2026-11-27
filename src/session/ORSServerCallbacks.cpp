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
  ::ors::log::warn("ors.session.disconnected",
                   {{"type", "session"},
                    {"venue_id", config_.venueId},
                    {"account_id", config_.accountId},
                    {"ors_id", config_.orsId},
                    {"error_code", std::to_string(nReason)},
                    {"message", "CTP front disconnected"}});
}

void ORSServer::OnRspError(CThostFtdcRspInfoField *pRspInfo, int, bool) {
  IsErrorRspInfo(pRspInfo);
}

void ORSServer::OnHeartBeatWarning(int nTimeLapse) {
  ::ors::log::warn("ors.session.heartbeat_warning",
                   {{"type", "session"},
                    {"venue_id", config_.venueId},
                    {"account_id", config_.accountId},
                    {"duration_us", static_cast<double>(nTimeLapse) * 1000000.0},
                    {"message", "CTP heartbeat warning"}});
}

bool ORSServer::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo) {
  const bool failed = pRspInfo != nullptr && pRspInfo->ErrorID != 0;
  if (failed) {
    ::ors::log::error("ors.session.error",
                      {{"type", "session"},
                       {"venue_id", config_.venueId},
                       {"account_id", config_.accountId},
                       {"error_code", std::to_string(pRspInfo->ErrorID)},
                       {"error_message", pRspInfo->ErrorMsg},
                       {"message", "CTP error"}});
  }
  return failed;
}

void ORSServer::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
                               CThostFtdcRspInfoField *pRspInfo, int,
                               bool bIsLast) {
  if (!bIsLast || IsErrorRspInfo(pRspInfo)) {
    ::ors::log::error("ors.session.login_failed",
                      {{"type", "session"},
                       {"venue_id", config_.venueId},
                       {"account_id", config_.accountId},
                       {"error_code", pRspInfo == nullptr ? "" : std::to_string(pRspInfo->ErrorID)},
                       {"error_message", pRspInfo == nullptr ? "" : pRspInfo->ErrorMsg},
                       {"message", "trader login failed"}});
    return;
  }

  orderService_.setNextOrderRef(std::atoi(pRspUserLogin->MaxOrderRef));
  FRONT_ID = pRspUserLogin->FrontID;
  SESSION_ID = pRspUserLogin->SessionID;

  if (m_pUserMDSpi != nullptr &&
      !m_pUserMDSpi->waitForLogin(std::chrono::seconds(3))) {
    ::ors::log::error("ors.session.login_failed",
                      {{"type", "session"},
                       {"venue_id", config_.venueId},
                       {"account_id", config_.accountId},
                       {"error_code", "md_login_timeout"},
                       {"error_message", "MD login timeout"},
                       {"message", "ORS startup aborted"}});
    return;
  }

  ReqSettlementInfoConfirm();
  rms_.impl().initRmsImpl(m_maxClients, m_rmsConfigFile, m_rmsFirmwideConfigFile, "");
  rmsReady_ = true;
  m_logonResp = true;
}

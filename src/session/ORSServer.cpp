#include <errno.h>
#include <string.h>
#include <iostream>
#include <utility>
#include <assert.h>
#include "session/ORSServer.h"
#include "infra/LogFacade.h"
#include "infra/Metrics.h"
#include "infra/Time.h"
#include "sstream"
#include "rmsimpl.hpp"
#include "query/QueryMapping.h"
#include "execution/OpenOrderGuard.h"
#include "execution/OffsetPolicy.h"

using namespace std;
using namespace ors::infra;
using namespace ors;
using namespace ors::md;

#define BUY 'B'
#define SELL 'S'

#define ERROR_CODE_CROSS_REJECT 1000101

#define OPEN_ORDER 3
#define CLOSE_TODAY 1
#define CLOSE_YESTERDAY 2

// int shmread ;
int mdupdate;
int rmscheck;

int newConfirmSHFE;
int newConfirmDCE;
int newConfirmZCE;
int newConfirmCFFEX;
int cancelConfirmSHFE;
int cancelConfirmDCE;
int cancelConfirmZCE;
int cancelConfirmCFFEX;

int orsLatency;
int crosscheck;
int orsdecode;
int apiLatency;
int respEnqueue;

uint64_t sendTime = 0;
uint64_t lastRecordTime = 0;

extern char ppInstrumentID[200][30];
extern int iInstrumentID;

/******************* ADMIN STUFF ****************************/

void ORSServer::registerStats()
{
    ::ors::metrics().init(
        config_.logPath + std::string{});
    // shmread = ::ors::metrics().registerStat( "ShmRead", 50, 200 ) ;
    orsLatency = ::ors::metrics().registerStat("MsgCreation", 2000, 5000);

    newConfirmSHFE = ::ors::metrics().registerStat("New SHFE", 2500000,
                                                       6000000);
    cancelConfirmSHFE = ::ors::metrics().registerStat("Cancel SHFE",
                                                          2500000, 6000000);
    newConfirmDCE = ::ors::metrics().registerStat("New DCE", 25000000,
                                                      30000000);
    cancelConfirmDCE = ::ors::metrics().registerStat("Cancel DCE", 25000000,
                                                         30000000);
    newConfirmZCE = ::ors::metrics().registerStat("New ZCE", 200000000,
                                                      400000000);
    cancelConfirmZCE = ::ors::metrics().registerStat("Cancel ZCE",
                                                         200000000, 400000000);
    newConfirmCFFEX = ::ors::metrics().registerStat("New CFFEX", 3000000,
                                                        6000000);
    cancelConfirmCFFEX = ::ors::metrics().registerStat("Cancel CFFEX",
                                                           3000000, 6000000);

    rmscheck = ::ors::metrics().registerStat("RMS", 2000, 5000);
    crosscheck = ::ors::metrics().registerStat("checkCross", 300, 500);
    orsdecode = ::ors::metrics().registerStat("MsgDecode", 2000, 10000);
    apiLatency = ::ors::metrics().registerStat("ApiLatency", 2000, 20000);
    respEnqueue = ::ors::metrics().registerStat("ResponseEnqueue", 1000,
                                                    20000);
}

ORSServer::ORSServer(::ors::OrsConfig config, ::ors::OrsTransport& transport,
                     MDHandler& mdHandler,
                     ::ors::md::MarketUpdateNew& marketUpdate)
    : config_(std::move(config)),
      transport_(transport),
      mdHandler_(mdHandler),
      marketUpdate_(marketUpdate) {
    m_iRequestID = 0;
    iNextOrderRef = 0;
    m_connected = false;

    m_rmsConfigFile = config_.rmsConfigFile;
    m_rmsFirmwideConfigFile = config_.rmsFirmwideConfigFile;

    registerStats();

    lastRecordTime = ::ors::nowNanos();
}

ORSServer::~ORSServer()
{
    if (m_connected)
    {
        Stop();
    }
}

void ORSServer::Start()
{
    {
        std::unique_lock<std::mutex> lock(readyMutex_);
        readyCv_.wait(lock, [this] {
            return m_connected.load() || stopRequested_.load();
        });
    }
    if (stopRequested_.load() || !m_connected.load()) {
        return;
    }

    accountQueryTimer_ = std::thread([this] {
        while (!stopRequested_.load() && m_connected.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const auto expired = accountQueryState_.expire(
                std::chrono::steady_clock::now());
            if (!expired.has_value()) {
                continue;
            }
            ::ors::AccountSnapshot snapshot;
            snapshot.requestId = *expired;
            snapshot.accountId = config_.accountId;
            snapshot.errorCode = 4;
            snapshot.errorMsg = "account query timeout";
            snapshot.timestamp = ::ors::nowNanos();
            transport_.sendAccount(snapshot);
        }
    });

    installSignals();
    HandleSignals();
}

void ORSServer::Stop()
{
    if (stopRequested_.exchange(true)) {
        return;
    }
    const bool wasConnected = m_connected.exchange(false);
    readyCv_.notify_all();
    if (accountQueryTimer_.joinable()) {
        accountQueryTimer_.join();
    }
    if (wasConnected) {
        ORS_LOG(INFO) << "Logging off\n";
        DoLoggOff();
    }
    ORS_LOG(INFO) << "Exiting\n";
}

void ORSServer::RegisterClient(const ::ors::ClientRegisterRequest& request)
{
    ::ors::ClientRegisterResult result;
    result.requestId = request.requestId;
    result.timestamp = ::ors::nowNanos();

    const ::ors::ClientRegistration registration =
        clientRegistry_.registerClient(request.instanceId);
    result.clientId = registration.clientId;
    result.errorCode = registration.errorCode;
    result.errorMsg = registration.errorMessage;
    clientRegistered_ = registration.ok;
    transport_.sendClientRegisterResponse(result);
}

void ORSServer::QueryAccount(const ::ors::AccountQueryRequest& request)
{
    auto sendError = [this, &request](std::uint32_t code, const char* message) {
        ::ors::AccountSnapshot snapshot;
        snapshot.requestId = request.requestId;
        snapshot.accountId = config_.accountId;
        snapshot.errorCode = code;
        snapshot.errorMsg = message;
        snapshot.timestamp = ::ors::nowNanos();
        transport_.sendAccount(snapshot);
    };

    if (!m_connected) {
        sendError(1, "ORS not ready");
        return;
    }
    if (!accountQueryState_.tryBegin(
            request.requestId, std::chrono::steady_clock::now(),
            std::chrono::milliseconds(config_.accountQueryTimeoutMs))) {
        sendError(3, "account query busy or throttled");
        return;
    }

    CThostFtdcQryTradingAccountField query;
    memset(&query, 0, sizeof(query));
    strcpy(query.BrokerID, m_BROKER_ID.c_str());
    strcpy(query.InvestorID, m_INVESTOR_ID.c_str());
    const int ctpRequestId = ++m_iRequestID;
    accountQueryState_.bindCtpRequest(ctpRequestId);
    if (m_pUserTradeApi->ReqQryTradingAccount(&query, ctpRequestId) != 0) {
        accountQueryState_.finish();
        sendError(2, "CTP account query rejected");
    }
}

void ORSServer::OnRspQryTradingAccount(
    CThostFtdcTradingAccountField *pAccount,
    CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    const auto sdkRequestId = accountQueryState_.requestIdFor(nRequestID);
    if (!sdkRequestId.has_value()) {
        return;
    }

    auto error = [this, &sdkRequestId](std::uint32_t code, const char* message) {
        ::ors::AccountSnapshot snapshot;
        snapshot.requestId = *sdkRequestId;
        snapshot.accountId = config_.accountId;
        snapshot.errorCode = code;
        snapshot.errorMsg = message;
        snapshot.timestamp = ::ors::nowNanos();
        transport_.sendAccount(snapshot);
        accountQueryState_.finish();
    };

    if (pRspInfo != nullptr && pRspInfo->ErrorID != 0) {
        error(static_cast<std::uint32_t>(pRspInfo->ErrorID), pRspInfo->ErrorMsg);
        return;
    }
    if (!bIsLast) {
        return;
    }
    if (pAccount == nullptr) {
        error(5, "empty account query response");
        return;
    }

    ::ors::AccountSnapshot snapshot =
        ::ors::mapAccountSnapshot(*pAccount, *sdkRequestId, config_);
    snapshot.timestamp = ::ors::nowNanos();
    transport_.sendAccount(snapshot);
    accountQueryState_.finish();
}

void ORSServer::RmsError(ResponseType resptype, uint32_t orderid,
                         uint32_t errorNo)
{
    ResponseMsg *resp = new ResponseMsg();
    resp->Response_Type = resptype;
    resp->OrderID = orderid;
    resp->ErrorCode = errorNo;
    resp->TimeStamp = ::ors::nowNanos();
    transport_.sendResponse(*resp);
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "RMS error | orderID: " << orderid << " | errorNo: " << errorNo << std::endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << resp->toString() << endl;
    delete resp;
}

void ORSServer::RmsWarning(ResponseType resptype, uint32_t orderid,
                           uint32_t errorNo)
{
    ResponseMsg *resp = new ResponseMsg();
    resp->Response_Type = resptype;
    resp->OrderID = orderid;
    resp->ErrorCode = errorNo;
    resp->TimeStamp = ::ors::nowNanos();
    transport_.sendResponse(*resp);
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "RMS Warning | orderID: " << orderid << " | errorNo: " << errorNo << std::endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << resp->toString() << endl;
    delete resp;
}

void ORSServer::DoLoggOff()
{
    int iResult;
    CThostFtdcUserLogoutField pUserLogout;
    strcpy(pUserLogout.BrokerID, m_BROKER_ID.c_str());
    strcpy(pUserLogout.UserID, m_INVESTOR_ID.c_str());
    m_logoffResp = false;
    iResult = m_pUserTradeApi->ReqUserLogout(&pUserLogout, ++m_iRequestID);
    if (!iResult)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Logout sent" << std::endl;
        // while (!m_logoffResp) {
        // }
    }
    else
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Logout sent error " << iResult << std::endl;
    }
}

void ORSServer::Login()
{
    std::string FRONT_ADDR_ORS = config_.tradeFront;
    std::string FRONT_ADDR_MD = config_.mdFront;
    std::string MULTICAST_ADDR;

    m_BROKER_ID = config_.brokerId;
    m_INVESTOR_ID = config_.investorId;
    m_INVESTOR_ID_SHFE = config_.investorId;
    m_INVESTOR_ID_CFFEX = config_.investorId;
    m_INVESTOR_ID_DCE = config_.investorId;
    m_INVESTOR_ID_ZCE = config_.investorId;
    m_PASSWORD = config_.password;
    m_NEWPASSWORD.clear();
    m_AUTHCODE = config_.authCode;
    m_APPID = config_.appId;

    if (m_pUserTradeApi == nullptr) {
        m_pUserTradeApi.reset(CThostFtdcTraderApi::CreateFtdcTraderApi(
            config_.traderFlowDir.c_str()));
    }
    if (m_pUserMDApi == nullptr) {
        m_pUserMDApi.reset(CThostFtdcMdApi::CreateFtdcMdApi(
            config_.mdFlowDir.c_str()));
    }

    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "Logging in to MD " << FRONT_ADDR_MD << "|" << m_BROKER_ID << "|" << m_INVESTOR_ID << std::endl;
    // create MD classes
    m_pUserMDSpi = std::make_unique<CMdSpi>(
        m_pUserMDApi.get(), *this, mdHandler_, marketUpdate_);
    m_pUserMDApi->RegisterSpi(m_pUserMDSpi.get());
    m_pUserMDApi->RegisterFront((char *)FRONT_ADDR_MD.c_str());
    // m_pUserMDSpi->ReqUserLogin((char*)m_BROKER_ID.c_str(), (char*)m_INVESTOR_ID.c_str(), (char*)m_PASSWORD.c_str());
    m_pUserMDApi->Init();

    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "Logging in to ORS " << FRONT_ADDR_ORS << "|" << m_BROKER_ID << "|" << m_INVESTOR_ID << std::endl;
    // create trade classes
    m_pUserTradeApi->RegisterSpi((CThostFtdcTraderSpi *)this);
    // we will log trades and re-consile orders
    // m_pUserTradeApi->SubscribePrivateTopic(THOST_TERT_RESTART);
    m_pUserTradeApi->SubscribePrivateTopic(THOST_TERT_QUICK);
    // m_pUserTradeApi->SubscribePublicTopic(THOST_TERT_QUICK);
    m_pUserTradeApi->RegisterFront((char *)FRONT_ADDR_ORS.c_str());
    m_pUserTradeApi->Init();

}

void ORSServer::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    if (bIsLast && !IsErrorRspInfo(pRspInfo))
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "User logged out successfully" << std::endl;
    else
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "User logged out failure" << std::endl;
    m_logoffResp = true;
}

/******************************* SENDING ORDERS ***************************/
void ORSServer::ProcessRequest(RequestMsg *req)
{
    RequestMsgSerializer rs(*req);
    (void)rs;
    if (!clientRegistered_) {
        RejectOrder(req, "ClientRegister required");
        return;
    }
    if (!rmsReady_) {
        RejectOrder(req, "RMS not ready");
        return;
    }
    switch (req->Request_Type)
    {
    case NEWORDER:
        SendNewOrder(req);
        break;
    case MODIFYORDER:
        SendModifyOrder(req);
        break;
    case CANCELORDER:
        SendCancelOrder(req);
        break;
    }
}

void ORSServer::SetCombOffsetFlag(CThostFtdcInputOrderField &req, RequestMsg *request, char &flag, int &orderType)
{
    const ::ors::OffsetReservation reservation = positionTable_.reserveForOrder(
        req.InstrumentID, request->Transaction_Type, request->Quantity,
        request->Exchange_Type == CHINA_SHFE);

    if (!reservation.valid) {
        flag = THOST_FTDC_OF_Open;
        orderType = OPEN_ORDER;
        return;
    }

    const ::ors::OffsetDecision decision =
        ::ors::chooseOffset(reservation, request->Exchange_Type);
    flag = decision.flag;
    orderType = decision.orderType;
}

void ORSServer::SendNewOrder(RequestMsg *request)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "NewOrder ------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << request->OrderID << std::endl;
    int iResult;
    CThostFtdcInputOrderField req;
    ::ors::OrderInfo ordinfo;

    uint64_t rmstime, ordercrosscheck, apisend;
    uint64_t curtime = ::ors::nowNanos();
    memset(&req, 0, sizeof(req));

    if (!rms.executeNewOrderChecks(*request))
    {
        ORS_LOG(INFO) << "NewOrder ------> "
                   << "NewOrder check failed. orderID:" << request->OrderID << std::endl;
        return;
    }
    rmstime = ::ors::nowNanos();

    ordercrosscheck = ::ors::nowNanos();

    if (request->OrdType == MARKET)
    {

        // defaults:
        req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
        req.LimitPrice = 0;
        req.TimeCondition = THOST_FTDC_TC_IOC;
        req.VolumeTotalOriginal = request->Quantity;
        req.VolumeCondition = THOST_FTDC_VC_CV;
        req.MinVolume = request->Quantity;
        if ((request->Duration == ::ors::infra::FOK) || (request->Duration == ::ors::infra::IOC))
        {
            if ((request->Exchange_Type == CHINA_CFFEX) || (request->Exchange_Type == CHINA_ZCE))
            {
                RejectOrder(request, "Order type not supported in this market");
                return;
            }
            req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
            req.LimitPrice = 0;
            req.TimeCondition = THOST_FTDC_TC_IOC;
            req.VolumeTotalOriginal = request->Quantity;
            req.VolumeCondition = THOST_FTDC_VC_CV;
            req.MinVolume = request->Quantity;
        }
        else if (request->Duration == ::ors::infra::FAK)
        {
            if ((request->Exchange_Type == CHINA_CFFEX) || (request->Exchange_Type == CHINA_ZCE))
            {
                RejectOrder(request, "Order type not supported in this market");
                return;
            }
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
        else
        {
            // TODO : check price type
            req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
            req.LimitPrice = 0;
            req.TimeCondition = THOST_FTDC_TC_GFD;
            req.VolumeTotalOriginal = request->Quantity;
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
    }
    else if (request->OrdType == LIMIT)
    {
        // defaults
        req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        req.LimitPrice = request->Price;
        req.TimeCondition = THOST_FTDC_TC_IOC;
        req.VolumeTotalOriginal = request->Quantity;
        req.VolumeCondition = THOST_FTDC_VC_CV;
        req.MinVolume = request->Quantity;

        if ((request->Duration == ::ors::infra::FOK) || (request->Duration == ::ors::infra::IOC))
        {
            if ((request->Exchange_Type == CHINA_CFFEX) || (request->Exchange_Type == CHINA_ZCE))
            {
                RejectOrder(request, "Order type not supported in this market");
                return;
            }

            req.VolumeCondition = THOST_FTDC_VC_CV;
            req.MinVolume = request->Quantity;
        }
        else if (request->Duration == ::ors::infra::FAK)
        {
            // if(!strcmp(symbol->second.ExchangeID, "CFFEX") || !strcmp(symbol->second.ExchangeID, "ZCE"))
            if ((request->Exchange_Type == CHINA_CFFEX) || (request->Exchange_Type == CHINA_ZCE))
            {
                RejectOrder(request, "Order type not supported in this market");
                return;
            }
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
        else
        {
            req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
            req.LimitPrice = request->Price;
            req.TimeCondition = THOST_FTDC_TC_GFD;
            req.VolumeTotalOriginal = request->Quantity;
            req.VolumeCondition = THOST_FTDC_VC_AV;
            req.MinVolume = 1;
        }
    }
    else
    {
        req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        req.LimitPrice = request->Price;
        req.TimeCondition = THOST_FTDC_TC_GFD;
        req.VolumeTotalOriginal = request->Quantity;
        req.VolumeCondition = THOST_FTDC_VC_AV;
        req.MinVolume = 1;
    }

    strcpy(req.BrokerID, m_BROKER_ID.c_str());
    strcpy(req.InvestorID, m_INVESTOR_ID.c_str());
    strcpy(req.InstrumentID, request->Contract_Description.Symbol);
    sprintf(ORDER_REF, "%012d", iNextOrderRef);
    strcpy(req.OrderRef, ORDER_REF);
    iNextOrderRef++;

    if (request->Transaction_Type == BUY)
    {
        req.Direction = THOST_FTDC_D_Buy;
    }
    else if (request->Transaction_Type == SELL)
    {
        req.Direction = THOST_FTDC_D_Sell;
    }
    else
    {
        RejectOrder(request, "New order Send Error");
        return;
    }
    /*
     switch(request->getDirection())
     {
     case CLOSE:                 req.CombOffsetFlag[0] = THOST_FTDC_OF_Close; req.CombOffsetFlag[1] = '\0'; 	break;
     case CLOSE_INTRADAY:
     if(request->Exchange_Type == CHINA_SHFE)
     {
     req.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday; req.CombOffsetFlag[1] = '\0';
     }
     else
     {
     req.CombOffsetFlag[0] = THOST_FTDC_OF_Close; req.CombOffsetFlag[1] = '\0';
     }
     break;
     case OPEN:                  req.CombOffsetFlag[0] = THOST_FTDC_OF_Open;	req.CombOffsetFlag[1] = '\0'; break;
     default :
     std::cout << "Reject order as direction not set" << std::endl;
     RejectOrder( request, "New order reject\n");
     return;
     }
     */

    char flag;
    int iOrderType = 0;
    SetCombOffsetFlag(req, request, flag, iOrderType);

    req.CombOffsetFlag[0] = flag;
    req.CombOffsetFlag[1] = '\0';
    req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
    req.CombHedgeFlag[1] = '\0';
    req.ContingentCondition = THOST_FTDC_CC_Immediately;
    req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
    req.IsAutoSuspend = 0;
    req.UserForceClose = 0;
    req.RequestID = m_iRequestID + 1;
    apisend = ::ors::nowNanos();
    iResult = m_pUserTradeApi->ReqOrderInsert(&req, ++m_iRequestID);
    if (iResult != 0)
    {
        RejectOrder(request, "New order Send Error");
        /*
         iNextOrderRef--;
         sprintf(ORDER_REF, "%d", iNextOrderRef);
         */
        return;
    }

    uint64_t endtime = ::ors::nowNanos();

    // maintain map here
    ordinfo.requestType = request->Request_Type;
    ordinfo.orderSent = 1;
    ordinfo.localID = request->OrderID;
    strcpy(ordinfo.exchID, ORDER_REF);
    strcpy(ordinfo.ClientID, request->AccountID);
    strcpy(ordinfo.ScripCode, request->Contract_Description.Symbol);
    ordinfo.Transaction_Type = request->Transaction_Type;
    ordinfo.Price = request->Price;
    ordinfo.Qty = request->Quantity;
    ordinfo.ModQtySent = 0;
    ordinfo.oldPrice = 0;
    ordinfo.oldQty = 0;
    ordinfo.openQty = request->Quantity;
    ordinfo.exchange = request->Exchange_Type;
    ordinfo.sendTime = endtime;
    ordinfo.frontId = FRONT_ID;
    ordinfo.sessionId = SESSION_ID;
    ordinfo.qtyType = iOrderType;

    orders_.localMutex().lock();
    orders_.local()[request->OrderID] = ordinfo;
    orders_.localMutex().unlock();

    orders_.exchangeMutex().lock();
    orders_.exchange()[std::string(req.OrderRef)] = request->OrderID;
    orders_.exchangeMutex().unlock();

    // ::ors::metrics().record( shmread, ((int64_t)curtime - (int64_t)request->TimeStamp)) ;
    ::ors::metrics().record(orsLatency,
                             ((int64_t)apisend - (int64_t)ordercrosscheck));
    ::ors::metrics().record(apiLatency,
                             ((int64_t)endtime - (int64_t)apisend));
    ::ors::metrics().record(rmscheck, ((int64_t)rmstime - (int64_t)curtime));
    ::ors::metrics().record(crosscheck,
                             ((int64_t)ordercrosscheck - (int64_t)rmstime));
    tempTimeStamp = ::ors::nowNanos();
    //  ORS_LOG(INFO) << "--->>> "<< tempTimeStamp << "|" << "New Order placed | " << request->Contract_Description.Symbol << " " << request->OrderID  << " " << ORDER_REF << " " << request->Price << " " << request->Quantity << std::endl ;
    // ORS_LOG(INFO) << "------> "<< tempTimeStamp << "|" << __FUNCTION__  << " end OrderRef"<< request->OrderID << std::endl;
}

void ORSServer::SendModifyOrder(RequestMsg *request)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    RejectOrder(request, "Modify orders not allowed");
    return;
}

void ORSServer::SendCancelOrder(RequestMsg *request)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << request->OrderID << std::endl;
    CThostFtdcInputOrderActionField req;
    ::ors::OrderInfo ordinfo;
    int iResult;
    uint64_t rmstime, apisend;
    uint64_t curtime = ::ors::nowNanos();

    memset(&req, 0, sizeof(req));

    orders_.localMutex().lock();
    if (orders_.local().find(request->OrderID) == orders_.local().end())
    {
        orders_.localMutex().unlock();
        RejectOrder(request, "New not found for Cancel request");
        return;
    }
    ordinfo = orders_.local()[request->OrderID];
    orders_.localMutex().unlock();

    if (!rms.executeCancelOrderChecks(*request))
    {
        return;
    }
    rmstime = ::ors::nowNanos();

    strcpy(req.BrokerID, m_BROKER_ID.c_str());
    // strcpy(req.UserID, m_BROKER_ID.c_str());
    strcpy(req.InvestorID, m_INVESTOR_ID.c_str());
    strcpy(req.InstrumentID, request->Contract_Description.Symbol);
    strcpy(req.OrderRef, ordinfo.exchID);
    req.FrontID = FRONT_ID;
    req.SessionID = SESSION_ID;
    req.ActionFlag = THOST_FTDC_AF_Delete;
    req.RequestID = m_iRequestID + 1;
    req.LimitPrice = request->Price;
    req.VolumeChange = request->Quantity;
    switch (request->Exchange_Type)
    {
    case CHINA_SHFE:
        strcpy(req.ExchangeID, "SHFE");
        break;
    case CHINA_CFFEX:
        strcpy(req.ExchangeID, "CFFEX");
        break;
    case CHINA_ZCE:
        strcpy(req.ExchangeID, "CZCE");
        break;
    case CHINA_DCE:
        strcpy(req.ExchangeID, "DCE");
        break;
    }
    strcpy(req.OrderSysID, ordinfo.OrderSysID);

    apisend = ::ors::nowNanos();
    ORS_LOG(INFO) << "After API call 4\n";
    iResult = m_pUserTradeApi->ReqOrderAction(&req, ++m_iRequestID);
    ORS_LOG(INFO) << "After API call 4\n";
    if (iResult != 0)
    {
        RejectOrder(request, "Cancel order send error");
        return;
    }
    uint64_t endtime = ::ors::nowNanos();

    ordinfo.ModQtySent = 0;
    ordinfo.requestType = CANCELORDER; // request->Request_Type;
    ordinfo.sendTime = endtime;

    orders_.localMutex().lock();
    orders_.local()[request->OrderID] = ordinfo;
    orders_.localMutex().unlock();

    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
               << "Cancel order sent | " << ordinfo.localID << " | " << ordinfo.exchID << '|' << request->Price << '|' << request->Quantity << std::endl;
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << request->OrderID << std::endl;

    // ::ors::metrics().record( shmread, ((int64_t)curtime - (int64_t)request->TimeStamp)) ;
    ::ors::metrics().record(orsLatency,
                             ((int64_t)apisend - (int64_t)rmstime));
    ::ors::metrics().record(apiLatency,
                             ((int64_t)endtime - (int64_t)apisend));
    ::ors::metrics().record(rmscheck, ((int64_t)rmstime - (int64_t)curtime));
}

void ORSServer::logOrderInfoResponse(const char *s, ::ors::OrderInfo &ordinfo,
                                     ResponseMsg *resp)
{
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << s << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.OrderSysID << " | " << ordinfo.ScripCode << " | " << ordinfo.Price << " | " << ordinfo.Qty << " | " << (resp->TimeStamp - ordinfo.sendTime) << std::endl;
}

void ORSServer::PushUpdate(ResponseMsg *resp)
{
    resp->TimeStamp = ::ors::nowNanos();
    // m_responseInterface->lock();
    transport_.sendResponse(*resp);
    ::ors::metrics().record(respEnqueue,
                             ::ors::nowNanos() - resp->TimeStamp);
    // m_responseInterface->unlock();

    ::ors::OrderInfo ordinfo;
    // if(orders_.local().find(resp->OrderID) != orders_.local().end())
    orders_.localMutex().lock();
    ordinfo = orders_.local()[resp->OrderID];
    orders_.localMutex().unlock();
    if (ordinfo.sendTime && resp->Response_Type != TRADE_CONFIRM)
    {
        switch (resp->Response_Type)
        {
            tempTimeStamp = ::ors::nowNanos();
        case NEW_ORDER_CONFIRM:
        case ORDER_ERROR:
            switch (ordinfo.exchange)
            {
            case CHINA_SHFE:
                logOrderInfoResponse("NC | SHFE", ordinfo, resp);
                ::ors::metrics().record(newConfirmSHFE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_CFFEX:
                logOrderInfoResponse("NC | CFFEX", ordinfo, resp);
                ::ors::metrics().record(newConfirmCFFEX,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_ZCE:
                logOrderInfoResponse("NC | ZCE", ordinfo, resp);
                ::ors::metrics().record(newConfirmZCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_DCE:
                logOrderInfoResponse("NC | DCE", ordinfo, resp);
                ::ors::metrics().record(newConfirmDCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            }
            break;
        case CANCEL_ORDER_CONFIRM:
        case CANCEL_ORDER_REJECT:
            switch (ordinfo.exchange)
            {
            case CHINA_SHFE:
                logOrderInfoResponse("CC | SHFE", ordinfo, resp);
                ::ors::metrics().record(cancelConfirmSHFE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_CFFEX:
                logOrderInfoResponse("CC | CFFEX", ordinfo, resp);
                ::ors::metrics().record(cancelConfirmCFFEX,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_ZCE:
                logOrderInfoResponse("CC | ZCE", ordinfo, resp);
                ::ors::metrics().record(cancelConfirmZCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_DCE:
                logOrderInfoResponse("CC | DCE", ordinfo, resp);
                ::ors::metrics().record(cancelConfirmDCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            }
            break;
        }
    }

    // update RMS here
    if (rmsReady_) {
        rms.handleORSResponse(resp);
    }

    // to update Position map
    //
    updatePosition(ordinfo, resp);
}

void ORSServer::updatePosition(::ors::OrderInfo &ordinfo, ResponseMsg *resp)
{
    ::ors::PositionAction action = ::ors::PositionAction::Open;
    if (ordinfo.qtyType == CLOSE_TODAY) {
        action = ::ors::PositionAction::CloseToday;
    } else if (ordinfo.qtyType == CLOSE_YESTERDAY) {
        action = ::ors::PositionAction::CloseYesterday;
    }

    switch (resp->Response_Type) {
    case ORDER_ERROR:
        positionTable_.restoreOnFailure(ordinfo.ScripCode,
                                        ordinfo.Transaction_Type, action,
                                        static_cast<int>(ordinfo.Qty));
        break;
    case CANCEL_ORDER_CONFIRM:
        positionTable_.restoreOnFailure(ordinfo.ScripCode,
                                        ordinfo.Transaction_Type, action,
                                        resp->Quantity);
        break;
    case TRADE_CONFIRM:
        positionTable_.applyTrade(ordinfo.ScripCode, ordinfo.Transaction_Type,
                                  action, resp->Quantity);
        break;
    default:
        break;
    }
}

void ORSServer::RejectOrder(RequestMsg *req, const char *Reason)
{
    ResponseMsg response;
    if (req->Request_Type == NEWORDER)
    {
        // response->Response_Type = ORDER_ERROR ;
        response.Response_Type = ORS_REJECT;
        response.ErrorCode = ERROR_CODE_CROSS_REJECT;
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "New Order rejected | " << req->OrderID << " " << Reason << std::endl;
    }
    if (req->Request_Type == MODIFYORDER)
    {
        response.Response_Type = MODIFY_ORDER_REJECT;
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Modify Order rejected | " << req->OrderID << " " << Reason << std::endl;
    }
    if (req->Request_Type == CANCELORDER)
    {
        response.Response_Type = CANCEL_ORDER_REJECT;
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Cancel Order rejected | " << req->OrderID << " " << Reason << std::endl;
    }
    response.OrderID = req->OrderID;
    strcpy(response.AccountID, req->AccountID);
    PushUpdate(&response);
    return;
}

void ORSServer::OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus)
{
    MasterData data;
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << "InstrumentID : " << pInstrumentStatus->InstrumentID << " ExchangeID : " << pInstrumentStatus->ExchangeID << " InstrumentStatus : " << pInstrumentStatus->InstrumentStatus << std::endl;
    if (pInstrumentStatus == NULL)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "First param as NULL OnRspQryInstrument" << std::endl;
        return;
    }
    if (m_symbolMap.find(std::string(pInstrumentStatus->InstrumentID)) == m_symbolMap.end())
    {
        strcpy(data.ScripCode, pInstrumentStatus->InstrumentID);
        // data.RoundLot = pInstrumentStatus->VolumeMultiple;
        data.RoundLot = 1;
        data.DecimalLocator = 1;
        data.Factor = 1;
        data.DPRHigh = 0;
        data.DPRLow = 0;
        data.Price = 0;
        // strcpy(data.ExpiryDate, pInstrumentStatus->ExpireDate);
        // strcpy(data.InstrumentCode, pInstrumentStatus->InstrumentName);
        strcpy(data.OptionType, "XX");
        data.StrikePrice = -1;
        strcpy(data.ExchangeID, pInstrumentStatus->ExchangeID);
        // data.MaxMarketOrderVolume = pInstrumentStatus->MaxMarketOrderVolume;
        // data.MinMarketOrderVolume = pInstrumentStatus->MinMarketOrderVolume;
        // data.MaxLimitOrderVolume = pInstrumentStatus->MaxLimitOrderVolume;
        // data.MinLimitOrderVolume = pInstrumentStatus->MinLimitOrderVolume;
        // data.PriceTick = pInstrumentStatus->PriceTick;
        data.PriceTick = 0.1;
        data.IsTrading = true;
        data.NetPositon = 0;
        // strcpy(ppInstrumentStatusID[iInstrumentID++], pInstrumentStatus->InstrumentID);
        
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << data.ScripCode << "|" << data.RoundLot << "|" << data.DecimalLocator << "|" << data.ExpiryDate << "|" << data.ExchangeID << "|" << data.PriceTick << "|" << data.IsTrading << std::endl;
        m_symbolMap[std::string(pInstrumentStatus->InstrumentID)] = data;
    }
    /*
     if(bIsLast)
     {
     std::cout << "Last record found TODO write to file to keep track of contracts" << std::endl;
     //std::string symbolList("/home/piyush/ORS_CHINA/config/Scrips.bcp");
     if(fpContractFile.good())
     {
     fpContractFile.close();
     }
     rms.setExchangeInstrDefinitionMap(m_symbolMap, true, true ,true);
     }
     */
}

void ORSServer::ReqQryInstrument(char *szInstrumentID)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    CThostFtdcQryInstrumentField req;
    memset(&req, 0, sizeof(req));
    if (szInstrumentID == NULL)
    {
        strcpy(req.InstrumentID, "");
    }
    else
    {
        strcpy(req.InstrumentID, szInstrumentID);
    }
    int iResult = m_pUserTradeApi->ReqQryInstrument(&req, ++m_iRequestID);
    tempTimeStamp = ::ors::nowNanos();
    if (!iResult)
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument info query successful" << std::endl;
    else
    {
        ORS_LOG(ERROR) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument info query failed" << std::endl;
        return;
    }
}

void ORSServer::ReqQryPosition()
{
    CThostFtdcQryInvestorPositionField request;
    memset(&request, 0, sizeof(request));
    strcpy(request.BrokerID, m_BROKER_ID.c_str());
    strcpy(request.InvestorID, m_INVESTOR_ID.c_str());
    const int result = m_pUserTradeApi->ReqQryInvestorPosition(
        &request, ++m_iRequestID);
    if (result != 0) {
        ORS_LOG(INFO) << "Position query failed rc=" << result << std::endl;
    }
}

void ORSServer::ReqQryAccount()
{
    return; // modify by pqr 20230213
}

void ORSServer::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    MasterData data;
    if (pInstrument == NULL)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "First param as NULL OnRspQryInstrument" << std::endl;
        return;
    }
    if (!IsErrorRspInfo(pRspInfo))
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument ID: " << pInstrument->InstrumentID << std::endl;
        if (pInstrument->ProductClass == '1' && m_symbolMap.find(std::string(pInstrument->InstrumentID)) == m_symbolMap.end())
        {
            strcpy(data.ScripCode, pInstrument->InstrumentID);
            data.RoundLot = pInstrument->VolumeMultiple;
            data.DecimalLocator = 1;
            data.Factor = 1;
            data.DPRHigh = 0;
            data.DPRLow = 0;
            data.Price = 0;
            strcpy(data.ExpiryDate, pInstrument->ExpireDate);
            strcpy(data.InstrumentCode, pInstrument->InstrumentName);
            strcpy(data.OptionType, "XX");
            data.StrikePrice = -1;
            strcpy(data.ExchangeID, pInstrument->ExchangeID);
            data.MaxMarketOrderVolume = pInstrument->MaxMarketOrderVolume;
            data.MinMarketOrderVolume = pInstrument->MinMarketOrderVolume;
            data.MaxLimitOrderVolume = pInstrument->MaxLimitOrderVolume;
            data.MinLimitOrderVolume = pInstrument->MinLimitOrderVolume;
            data.PriceTick = pInstrument->PriceTick;
            data.IsTrading = pInstrument->IsTrading;
            data.NetPositon = 0;
            // strcpy(ppInstrumentID[iInstrumentID++], pInstrument->InstrumentID);
            
                tempTimeStamp = ::ors::nowNanos();
                ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << data.ScripCode << "|" << data.RoundLot << "|" << data.DecimalLocator << "|" << data.ExpiryDate << "|" << data.ExchangeID << "|" << data.PriceTick << "|" << data.IsTrading << std::endl;
            m_symbolMap[std::string(pInstrument->InstrumentID)] = data;
        }
    }
    else
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "OnRspQryInstrument error on callback" << std::endl;
    }
    if (bIsLast)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument query complete" << std::endl;
        m_pUserMDSpi->SubscribeMarketData();
        rms.setExchangeInstrDefinitionMap(m_symbolMap, true, true, false);
        ReqQryPosition();
    }
}

void ORSServer::OnRspQryInvestorPosition(
    CThostFtdcInvestorPositionField *pPosition,
    CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo != nullptr && pRspInfo->ErrorID != 0) {
        pendingPositionRows_.clear();
        ORS_LOG(INFO) << "Position query error: " << pRspInfo->ErrorID << " "
                   << pRspInfo->ErrorMsg << std::endl;
        return;
    }

    if (pPosition != nullptr) {
        ::ors::PositionRow row;
        row.instrumentId = pPosition->InstrumentID;
        row.side = pPosition->PosiDirection == THOST_FTDC_PD_Short
                       ? ::ors::PositionSide::Short
                       : ::ors::PositionSide::Long;
        row.position = pPosition->Position;
        row.todayPosition = pPosition->TodayPosition;
        pendingPositionRows_.push_back(std::move(row));
    }

    if (bIsLast) {
        positionTable_.replaceFromQuery(std::move(pendingPositionRows_));
        pendingPositionRows_.clear();
        m_connected.store(true);
        readyCv_.notify_all();
        ORS_LOG(INFO) << "Position query complete; ORS ready" << std::endl;
        ReqQryOrder();
    }
}

void ORSServer::ReqQryOrder()
{
    if (startupOrderQueryIssued_) {
        return;
    }
    startupOrderQueryIssued_ = true;
    CThostFtdcQryOrderField request;
    memset(&request, 0, sizeof(request));
    strcpy(request.BrokerID, m_BROKER_ID.c_str());
    strcpy(request.InvestorID, m_INVESTOR_ID.c_str());
    const int result = m_pUserTradeApi->ReqQryOrder(&request, ++m_iRequestID);
    if (result != 0) {
        ::ors::log::error("ors.startup.order_query_failed",
                          {{"severity", "P1"},
                           {"message", "CTP order query rejected"},
                           {"return_code", result}});
        std::cerr << "ORS_ALERT startup_order_query_failed return=" << result << '\n';
    }
}

void ORSServer::OnRspQryOrder(CThostFtdcOrderField *pOrder,
                              CThostFtdcRspInfoField *pRspInfo,
                              int, bool bIsLast)
{
    if (pRspInfo != nullptr && pRspInfo->ErrorID != 0) {
        pendingStartupOrders_.clear();
        ::ors::log::error("ors.startup.order_query_failed",
                          {{"severity", "P1"},
                           {"message", pRspInfo->ErrorMsg},
                           {"return_code", pRspInfo->ErrorID}});
        std::cerr << "ORS_ALERT startup_order_query_failed code="
                  << pRspInfo->ErrorID << '\n';
        return;
    }
    if (pOrder != nullptr) {
        pendingStartupOrders_.push_back(*pOrder);
    }
    if (!bIsLast) {
        return;
    }

    if (::ors::hasLiveOrders(pendingStartupOrders_)) {
        std::string refs;
        for (const auto &order : pendingStartupOrders_) {
            if (order.OrderStatus == THOST_FTDC_OST_AllTraded ||
                order.OrderStatus == THOST_FTDC_OST_Canceled) {
                continue;
            }
            if (!refs.empty()) {
                refs += ',';
            }
            refs += order.OrderRef;
            if (refs.size() >= 512) {
                break;
            }
        }
        ::ors::log::error("ors.startup.live_orders",
                          {{"severity", "P1"},
                           {"message", "CTP has live orders at startup"},
                           {"count", static_cast<int>(pendingStartupOrders_.size())},
                           {"order_refs", refs}});
        std::cerr << "ORS_ALERT startup_live_orders count="
                  << pendingStartupOrders_.size() << " refs=" << refs << '\n';
    } else {
        ::ors::log::info("ors.startup.orders_flat",
                         {{"message", "no live CTP orders at startup"}});
    }
    pendingStartupOrders_.clear();
}

void ORSServer::ReqSettlementInfoConfirm()
{
    CThostFtdcSettlementInfoConfirmField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.BrokerID, m_BROKER_ID.c_str());
    strcpy(req.InvestorID, m_INVESTOR_ID.c_str());
    int iResult = m_pUserTradeApi->ReqSettlementInfoConfirm(&req, ++m_iRequestID);
}

void ORSServer::Authenticate()
{
    CThostFtdcReqAuthenticateField authReq = {0};
    strncpy(authReq.BrokerID, m_BROKER_ID.c_str(), sizeof(TThostFtdcBrokerIDType));
    strncpy(authReq.UserID, m_INVESTOR_ID.c_str(), sizeof(TThostFtdcInvestorIDType));
    // strncpy(authReq.UserProductInfo, _userProductInfo.c_str(), sizeof(TThostFtdcProductInfoType));
    strncpy(authReq.AuthCode, m_AUTHCODE.c_str(), sizeof(TThostFtdcAuthCodeType));
    strncpy(authReq.AppID, m_APPID.c_str(), sizeof(TThostFtdcAppIDType));
    auto ret = m_pUserTradeApi->ReqAuthenticate(&authReq, m_iRequestID++);

    ORS_LOG(INFO) << "|ctp auth response:[" << ret << "], waiting for auth push" << std::endl;
    std::cout << "ctp auth response:[" << ret << "], waiting for auth push" << endl;
}

void ORSServer::ReqUserLogin()
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.BrokerID, m_BROKER_ID.c_str());
    strcpy(req.UserID, m_INVESTOR_ID.c_str());
    strcpy(req.Password, m_PASSWORD.c_str());
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "BrokerID " << req.BrokerID << " UserID " << req.UserID << std::endl;
    m_logonResp = false;
    int iResult = m_pUserTradeApi->ReqUserLogin(&req, ++m_iRequestID);
    if (!iResult)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Login send success" << std::endl;
    }
    else
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(ERROR) << "|" << tempTimeStamp << "|"
                   << "Login send failure" << std::endl;
        return;
    }
    // ReqUserLogin(req.BrokerID, req.UserID, req.Password);
}

void ORSServer::OnFrontConnected()
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "OnFrontConnected ORS" << std::endl;
    Authenticate();
    // ReqUserLogin();
}

void ORSServer::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "OnRspSettlementInfoConfirm" << std::endl;
    if (bIsLast && !IsErrorRspInfo(pRspInfo))
    {
        if (config_.instrument.empty()) {
            ReqQryInstrument(NULL);
        } else {
            ReqQryInstrument(const_cast<char*>(config_.instrument.c_str()));
        }
    }
}

bool ORSServer::OrderCrossCheck(RequestMsg *request)
{
    OrderAccountMapIter openorders;
    orders_.localMutex().lock();
    for (openorders = orders_.local().begin(); openorders != orders_.local().end();
         openorders++)
    {
        if (request->Transaction_Type != openorders->second.Transaction_Type)
        {
            // then these are opposite side orders
            // if(request->Exchange_Type == openorders->second.Exchange_Type)
            if (((openorders->second.Transaction_Type == BUY) && (request->Price <= openorders->second.Price)) || ((openorders->second.Transaction_Type == SELL) && (request->Price >= openorders->second.Price || request->Price == 0)))
            {
                // same exchange type
                // why do above so as to avoid many string compares
                if (!strcmp(request->Contract_Description.Symbol,
                            openorders->second.ScripCode))
                {
                    tempTimeStamp = ::ors::nowNanos();
                    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                               << "Cross : " << openorders->first << " | " << request->OrderID << std::endl;
                    orders_.localMutex().unlock();
                    return true;
                }
            }
        }
    }
    orders_.localMutex().unlock();
    return false;
}

/*
 {
 std::cout << __FUNCTION__ << std::endl;
 CSgitFtdcInputOrderActionField req;

 ::ors::OrderInfo ordinfo;
 int iResult;
 uint64_t rmstime, apisend;
 uint64_t curtime = ::ors::nowNanos() ;
 /*
 if (!rms.executeCancelOrderChecks(*request))
 {
 return;
 }
 *
 rmstime = ::ors::nowNanos();
 CSgitFtdcInputOrderActionField reqOrderAction = { 0 };

 if(orders_.local().find(request->OrderID) == orders_.local().end())
 {
 RejectOrder( request, "New not found for Cancel request\n") ;
 return;
 }
 ordinfo = orders_.local()[request->OrderID];
 sprintf(reqOrderAction.ExchangeID,"%s","S");
 sprintf(reqOrderAction.InstrumentID,"%s",request->Contract_Description.Symbol);
 sprintf(reqOrderAction.UserID,"%s", m_BROKER_ID.c_str());
 sprintf(reqOrderAction.InvestorID,"%s", m_INVESTOR_ID.c_str());
 sprintf(reqOrderAction.OrderSysID,"%s", ordinfo.OrderSysID);
 reqOrderAction.ActionFlag = Sgit_FTDC_AF_Delete;
 /*

 std::cout << "API values : UserID " << req.UserID << " InvestorID  " << req.InvestorID << " InstrumentID  " << req.InstrumentID
 << " OrderRef  " << req.OrderRef << " FrontID " << req.FrontID << " SessionID " << req.SessionID << " ActionFlag " << req.ActionFlag
 << " RequestID " << req.RequestID << " LimitPrice " << req.LimitPrice << " VolumeChange " << req.VolumeChange
 << " ExchangeID " << req.ExchangeID << " OrderSysID " << req.OrderSysID << std::endl;
 *

 apisend = ::ors::nowNanos() ;
 iResult = m_pUserTradeApi->ReqOrderAction(&reqOrderAction, ++m_iRequestID);
 if (iResult != 0)
 {
 RejectOrder( request, "Cancel order send error\n") ;
 return ;
 }
 uint64_t endtime = ::ors::nowNanos() ;

 ordinfo.ModQtySent = 0;
 ordinfo.requestType = request->Request_Type;
 ordinfo.sendTime = endtime;
 //ordinfo.orderSent = 1;
 orders_.local()[request->OrderID] = ordinfo;
 orders_.exchange()[std::string(req.OrderRef)] = ordinfo;

 std::cout << "Cancel order sent | " << ordinfo.localID << " | " << ordinfo.exchID << '|' << request->Price << '|' << request->Quantity << std::endl ;

 //::ors::metrics().record( shmread, ((int64_t)curtime - (int64_t)request->TimeStamp)) ;
 ::ors::metrics().record( orsLatency, ((int64_t)apisend - (int64_t)rmstime)) ;
 ::ors::metrics().record( apiLatency, ((int64_t)endtime - (int64_t)apisend)) ;
 ::ors::metrics().record( rmscheck, ((int64_t)rmstime - (int64_t)curtime)) ;
 }
 */

bool ORSServer::GetOrderInfo(const char *orderref, ::ors::OrderInfo &orderinfo)
{
    // TODO : temporary solution.. can do better
    uint32_t oid;
    orders_.exchangeMutex().lock();
    auto localID = orders_.exchange().find(std::string(orderref));
    if (localID == orders_.exchange().end())
    {
        ORS_LOG(FATAL) << "OrderRef: " << orderref << " not found in map\n";
        orders_.exchangeMutex().unlock();
        return false;
    }
    else
    {
        oid = localID->second;
    }
    orders_.exchangeMutex().unlock();

    orders_.localMutex().lock();

    auto oinfo_it = orders_.local().find(oid);
    if (oinfo_it == orders_.local().end())
    {
        ORS_LOG(FATAL) << "OrderID: " << oid << " not found in map\n";
        orders_.localMutex().unlock();
        return false;
    }
    else
    {
        orderinfo = oinfo_it->second;
    }

    orders_.localMutex().unlock();
    return true;
}

void ORSServer::RemoveFromMap(const char *exchid, int32_t localid)
{
    orders_.localMutex().lock();
    OrderAccountMapIter iter = orders_.local().find(localid);
    if (iter != orders_.local().end())
    {
        orders_.local().erase(iter);
    }
    orders_.localMutex().unlock();

    orders_.exchangeMutex().lock();
    OrderAccountMapIter2 iter2 = orders_.exchange().find(std::string(exchid));
    if (iter2 != orders_.exchange().end())
    {
        tempTimeStamp = ::ors::nowNanos();
        //   ORS_LOG(INFO) << "--->>> "<< tempTimeStamp << "|" << "Erased order from map " << localid << " | " << exchid << std::endl;
        orders_.exchange().erase(iter2);
    }
    orders_.exchangeMutex().unlock();
}


void ORSServer::OnRspOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (!bIsLast || pOrderAction == nullptr || pRspInfo == nullptr ||
        pRspInfo->ErrorID == 0) {
        return;
    }

    ::ors::OrderInfo order;
    if (!GetOrderInfo(pOrderAction->OrderRef, order)) {
        return;
    }
    if (pOrderAction->FrontID != order.frontId ||
        pOrderAction->SessionID != order.sessionId) {
        return;
    }

    ResponseMsg response;
    response.Response_Type = CANCEL_ORDER_REJECT;
    response.OrderID = order.localID;
    response.ErrorCode = pRspInfo->ErrorID;
    response.Price = pOrderAction->LimitPrice;
    response.Quantity = pOrderAction->VolumeChange;
    std::strncpy(response.AccountID, config_.accountId.c_str(), sizeof(response.AccountID) - 1);
    PushUpdate(&response);
}

void ORSServer::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef " << pInputOrder->OrderRef << " OrderSysID : " << pInputOrder->OrderRef << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    bool bRemoveOrder = false;
    // Changed since error code 0 is also valid in this case
    // if(pRspInfo->ErrorID != 0)
    {
        ResponseMsg response;
        ::ors::OrderInfo ordinfo;
        if (!GetOrderInfo(pInputOrder->OrderRef, ordinfo))
            return;

        response.OrderID = ordinfo.localID;
        response.Price = pInputOrder->LimitPrice;
        response.Quantity = pInputOrder->VolumeTotalOriginal;
        response.ErrorCode = pRspInfo->ErrorID;
        ordinfo.Price = pInputOrder->LimitPrice;
        // ordinfo.Qty = pInputOrder->VolumeTotalOriginal;

        strcpy(ordinfo.OrderSysID, pInputOrder->OrderRef);
        // switch(rms.getLastRequestOutgoingState (response))
        switch (ordinfo.requestType)
        {
        case NEWORDER:
            response.Response_Type = ORDER_ERROR;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = false;
            break;
        default:
            response.Response_Type = ORDER_ERROR;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unknown Reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        }

        PushUpdate(&response);
        if (bRemoveOrder)
        {
            RemoveFromMap(ordinfo.exchID, response.OrderID);
        }
        else
        {
            orders_.localMutex().lock();
            orders_.local()[response.OrderID] = ordinfo;
            orders_.localMutex().unlock();
        }
    }
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pInputOrder->OrderRef << std::endl;
}

void ORSServer::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef " << pOrderAction->OrderRef << " OrderSysID : " << pOrderAction->OrderSysID << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    bool bRemoveOrder = false;
    // Changed since error code 0 is also valid in this case
    // if(pRspInfo->ErrorID != 0)
    {
        ResponseMsg response;
        ::ors::OrderInfo ordinfo;
        if (!GetOrderInfo(pOrderAction->OrderRef, ordinfo))
            return;

        response.OrderID = ordinfo.localID;
        response.Price = pOrderAction->LimitPrice;
        response.Quantity = 0;
        response.ErrorCode = pRspInfo->ErrorID;
        ordinfo.Price = pOrderAction->LimitPrice;

        strcpy(ordinfo.OrderSysID, pOrderAction->OrderRef);
        // switch(rms.getLastRequestOutgoingState (response))
        switch (ordinfo.requestType)
        {
        case NEWORDER:
            response.Response_Type = ORDER_ERROR;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = false;
            break;
        default:
            response.Response_Type = ORDER_ERROR;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unknown Reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        }

        PushUpdate(&response);
        if (bRemoveOrder)
        {
            RemoveFromMap(ordinfo.exchID, response.OrderID);
        }
        else
        {
            orders_.localMutex().lock();
            orders_.local()[response.OrderID] = ordinfo;
            orders_.localMutex().unlock();
        }
    }
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrderAction->OrderRef << std::endl;
}

void ORSServer::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pInputOrder->OrderRef << std::endl;
    bool bRemoveOrder = false;
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << " OrderSysID : " << pInputOrder->OrderRef << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    // Changed since error code 0 is also valid in this case
    // if(pRspInfo->ErrorID != 0)
    {
        ResponseMsg response;
        ::ors::OrderInfo ordinfo;

        if (!GetOrderInfo(pInputOrder->OrderRef, ordinfo))
            return;

        response.OrderID = ordinfo.localID;
        response.Price = pInputOrder->LimitPrice;
        response.Quantity = pInputOrder->VolumeTotalOriginal;
        response.ErrorCode = pRspInfo->ErrorID;
        ordinfo.Price = pInputOrder->LimitPrice;
        // ordinfo.Qty = pInputOrder->VolumeTotalOriginal;

        strcpy(ordinfo.OrderSysID, pInputOrder->OrderRef);
        // switch(rms.getLastRequestOutgoingState (response))
        tempTimeStamp = ::ors::nowNanos();
        switch (ordinfo.requestType)
        {
        case NEWORDER:
            response.Response_Type = ORDER_ERROR;
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        default:
            response.Response_Type = ORDER_ERROR;
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unknown Reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        }

        PushUpdate(&response);
        if (bRemoveOrder)
        {
            RemoveFromMap(ordinfo.exchID, response.OrderID);
        }
        else
        {
            orders_.localMutex().lock();
            orders_.local()[response.OrderID] = ordinfo;
            orders_.localMutex().unlock();
        }
    }
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pInputOrder->OrderRef << std::endl;
}

void ORSServer::OnRtnOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pOrder->OrderRef << __FUNCTION__ << "OrderSysID : " << pOrder->OrderSysID << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    if (pRspInfo->ErrorID != 0 || pOrder->OrderSysID == NULL)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Order error, errorcode=" << pRspInfo->ErrorID << " errormsg= " << pRspInfo->ErrorMsg << std::endl;
        pOrder->OrderSubmitStatus = THOST_FTDC_OSS_InsertRejected - '0';
    }
    OnRtnOrder(pOrder);
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrder->OrderRef << std::endl;
}

void ORSServer::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pOrder->OrderRef << std::endl;
    ::ors::OrderInfo ordinfo;
    ResponseMsg response;
    bool bRemoveOrder = false;

    if (!(GetOrderInfo(pOrder->OrderRef, ordinfo)))
    {
        return;
    }
    if (pOrder->FrontID != ordinfo.frontId ||
        pOrder->SessionID != ordinfo.sessionId) {
        return;
    }

    response.OrderID = ordinfo.localID;
    response.Price = pOrder->LimitPrice;
    response.Quantity = pOrder->VolumeTotalOriginal;
    ordinfo.Price = pOrder->LimitPrice;

    strcpy(ordinfo.OrderSysID, pOrder->OrderSysID);

    switch (pOrder->OrderSubmitStatus)
    {
    case THOST_FTDC_OSS_InsertSubmitted:
        switch (pOrder->OrderStatus)
        {
        case THOST_FTDC_OST_AllTraded:
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "All traded | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << std::endl;
            // as trade might come late
            // bRemoveOrder = true;
            break;
        /*
         case THOST_FTDC_OST_PartTradedQueueing:
         break;
         case THOST_FTDC_OST_PartTradedNotQueueing:
         break;
         case THOST_FTDC_OST_NoTradeQueueing:
         break;
         case THOST_FTDC_OST_NoTradeNotQueueing:
         break;
         */
        case THOST_FTDC_OST_Canceled:
            response.Response_Type = CANCEL_ORDER_CONFIRM;
            response.Quantity = pOrder->VolumeTotal;
            PushUpdate(&response);
            ordinfo.Qty = ordinfo.Qty - pOrder->VolumeTotal;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel confirm | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << " | " << response.Quantity << std::endl;
            if (ordinfo.Qty == 0)
            {
                bRemoveOrder = true;
            }
            else
            {
                tempTimeStamp = ::ors::nowNanos();
                ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                           << "Partial cancel condition triggered2" << std::endl;
            }
            break;
        case THOST_FTDC_OST_Unknown:
            ordinfo.Qty = pOrder->VolumeTotalOriginal;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Request submitted | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        /*
         case THOST_FTDC_OST_NotTouched:
         break;
         case THOST_FTDC_OST_Touched:
         break;
         */
        default:
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "LocalID | " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unhandled order status | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        }
        break;
    case THOST_FTDC_OSS_Accepted:
        switch (pOrder->OrderStatus)
        {
        case THOST_FTDC_OST_AllTraded:
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "All traded | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            // bRemoveOrder = true;
            break;
        case THOST_FTDC_OST_PartTradedQueueing:
        case THOST_FTDC_OST_NoTradeQueueing:
            if (ordinfo.orderSent)
            {
                // ordinfo.Qty = pOrder->VolumeTotalOriginal;
                response.Response_Type = NEW_ORDER_CONFIRM;
                PushUpdate(&response);
                tempTimeStamp = ::ors::nowNanos();
                ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                           << "New confirm | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
                ordinfo.orderSent = 0;
            }
            break;
        /*
         case THOST_FTDC_OST_PartTradedNotQueueing:
         break;
         case THOST_FTDC_OST_NoTradeNotQueueing:
         break;
         */
        case THOST_FTDC_OST_Canceled:
            response.Response_Type = CANCEL_ORDER_CONFIRM;
            response.Quantity = pOrder->VolumeTotal;
            PushUpdate(&response);
            ordinfo.Qty = ordinfo.Qty - pOrder->VolumeTotal;
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel confirm | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            if (ordinfo.Qty == 0)
            {
                bRemoveOrder = true;
            }
            else
            {
                tempTimeStamp = ::ors::nowNanos();
                ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                           << "Partial cancel condition triggered" << std::endl;
            }
            break;
        /*
         case THOST_FTDC_OST_Unknown:
         break;
         case THOST_FTDC_OST_NotTouched:
         break;
         case THOST_FTDC_OST_Touched:
         break;
         */
        default:
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "LocalID | " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unhandled order status " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        }
        break;
    case THOST_FTDC_OSS_InsertRejected:
        switch (ordinfo.requestType)
        {
        case NEWORDER:
            response.Response_Type = ORDER_ERROR;
            PushUpdate(&response);
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            PushUpdate(&response);
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        default:
            // ordinfo.Qty = pOrder->VolumeTotalOriginal;
            response.Response_Type = ORDER_ERROR;
            PushUpdate(&response);
            tempTimeStamp = ::ors::nowNanos();
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "LocalID | " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
            ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unknown Reject " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        }
        break;
    default:
        /*
         * cases covered here
         * THOST_FTDC_OSS_CancelSubmitted
         * THOST_FTDC_OSS_ModifySubmitted
         * THOST_FTDC_OSS_CancelRejected
         * THOST_FTDC_OSS_ModifyRejected
         */
        // ordinfo.Qty = pOrder->VolumeTotalOriginal;
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "LocalID " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "unhandled status " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
        break;
    }

    // PushUpdate(&response) ;
    // std::cout << "OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;

    if (bRemoveOrder)
    {
        RemoveFromMap(ordinfo.exchID, response.OrderID);
    }
    else
    {
        orders_.localMutex().lock();
        orders_.local()[response.OrderID] = ordinfo;
        orders_.localMutex().unlock();
    }
    // UpdateRMS(res, ordinfo.ScripCode, ordinfo.ClientID, ordinfo.Transaction_Type);
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrder->OrderRef << std::endl;
}


void ORSServer::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
    tempTimeStamp = ::ors::nowNanos();
//	ORS_LOG(INFO) << "------> "<< tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef " << pTrade->OrderRef << std::endl;
    ::ors::OrderInfo ordinfo;
    ResponseMsg response;

    /*
     if (!GetOrderInfo(pTrade->OrderRef, ordinfo))
     return;

     ordinfo.Qty = ordinfo.Qty - pTrade->Volume;
     ordinfo.openQty = ordinfo.Qty;

     if (ordinfo.openQty > 0) {
     orders_.localMutex().lock();
     } else {
     RemoveFromMap(ordinfo.exchID, ordinfo.localID);
     }
     */
    // TODO: Didn't use Get::ors::OrderInfo directly because there was some unusal locking in this function
    uint32_t oid;
    orders_.exchangeMutex().lock();
    auto localID_it = orders_.exchange().find(std::string(pTrade->OrderRef));
    if (localID_it == orders_.exchange().end())
    {
        ORS_LOG(FATAL) << "OrderRef: " << pTrade->OrderRef << " not found in map\n";
        orders_.exchangeMutex().unlock();
        return;
    }
    else
    {
        oid = localID_it->second;
    }
    orders_.exchangeMutex().unlock();

    orders_.localMutex().lock();
    ordinfo = orders_.local()[oid];
    orders_.localMutex().unlock();
    orders_.localMutex().lock();
    ordinfo.Qty = ordinfo.Qty - pTrade->Volume;
    ordinfo.openQty = ordinfo.Qty;
    orders_.local()[oid] = ordinfo;
    orders_.localMutex().unlock();

    response.Response_Type = TRADE_CONFIRM;
    response.OrderID = ordinfo.localID;
    response.Price = pTrade->Price;
    response.Quantity = pTrade->Volume;
    strcpy(response.AccountID, ordinfo.ClientID);
    strcpy(response.Symbol, ordinfo.ScripCode);
    response.Side = ordinfo.Transaction_Type;

    PushUpdate(&response);

    ordinfo.orderSent = 0;

    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
               << "Trade | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << pTrade->Price << " | " << pTrade->Volume << " | " << ordinfo.openQty << std::endl;

    // UpdateRMS(res, ordinfo.ScripCode, ordinfo.ClientID, ordinfo.Transaction_Type);

    // Now this should be done by order response only
    if (ordinfo.openQty > 0)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Partially traded" << std::endl;
        // orders_.local()[response.OrderID] = ordinfo;
        // orders_.exchange()[std::string(ordinfo.exchID)] = ordinfo;
    }
    else
    {
        RemoveFromMap(ordinfo.exchID, response.OrderID);
    }
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef " << pTrade->OrderRef << std::endl;
}

void ORSServer::installSignals()
{
    ORS_LOG(INFO) << "Installing signals\n";
    sigfillset(&m_signalSet);
    sigaddset(&m_signalSet, SIGUSR1);
    sigaddset(&m_signalSet, SIGUSR2);
    sigaddset(&m_signalSet, SIGTSTP);
    sigaddset(&m_signalSet, SIGINT);
    sigaddset(&m_signalSet, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &m_signalSet, NULL) != 0)
    {
        ORS_LOG(FATAL) << "sigmask error: " << strerror(errno) << std::endl;
        throw("sigmask error");
    }
}

void ORSServer::HandleSignals()
{

    int32_t sig = 0;
    while (!stopRequested_.load())
    {
        ORS_LOG(INFO) << "Signal handler thread sleeping\n";
        if (sigwait(&m_signalSet, &sig) != 0)
        {
            ORS_LOG(FATAL) << "Sigwait error " << std::endl;
        }

        switch (sig)
        {
        case SIGTSTP: // ctrl+z
        {
            ORS_LOG(INFO) << "Attempting ORS normal exit." << std::endl;
            ORS_LOG(INFO) << "Shutting down the SHM manager \n";
            Stop();
            break;
        }
        case SIGTERM: // default by kill command
        {
            ORS_LOG(INFO) << "Attempting ORS normal exit." << std::endl;
            ORS_LOG(INFO) << "Shutting down the SHM manager \n";
            Stop();
            break;
        }
        case SIGINT: // ctrl+c
        {
            ORS_LOG(INFO) << "ORS  exiting." << std::endl;
            Stop();
            break;
        }
        case SIGUSR1:
        {
            ORS_LOG(INFO) << "Resetting RMS state." << std::endl;
            rms.reset();
            break;
        }
        case SIGUSR2:
        {
            ORS_LOG(INFO) << "Fetch positions " << std::endl;
            ReqQryPosition();
            break;
        }

        default:
        {

            ORS_LOG(INFO) << "Received Unhandled signal: " << sig << std::endl;
        }
        }
    }
}

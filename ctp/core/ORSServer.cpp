#include <errno.h>
#include <string.h>
#include <iostream>
#include <utility>
#include <assert.h>
#include "ORSServer.h"
#include "statscollector.h"
#include "sstream"
#include "rmsimpl.hpp"
#include "logger.h"
#include "processsettings.h"

using namespace std;
using namespace illuminati::infra;
using namespace illuminati;
using namespace illuminati::md;
using namespace illuminati::stats;

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
    StatsCollector<>::getInstance().init(
        config_.logPath + ITime_Logger::GetCurrentYYYYMMDD());
    // shmread = StatsCollector<>::registerNewStat( "ShmRead", 50, 200 ) ;
    orsLatency = StatsCollector<>::registerNewStat("MsgCreation", 2000, 5000);

    newConfirmSHFE = StatsCollector<>::registerNewStat("New SHFE", 2500000,
                                                       6000000);
    cancelConfirmSHFE = StatsCollector<>::registerNewStat("Cancel SHFE",
                                                          2500000, 6000000);
    newConfirmDCE = StatsCollector<>::registerNewStat("New DCE", 25000000,
                                                      30000000);
    cancelConfirmDCE = StatsCollector<>::registerNewStat("Cancel DCE", 25000000,
                                                         30000000);
    newConfirmZCE = StatsCollector<>::registerNewStat("New ZCE", 200000000,
                                                      400000000);
    cancelConfirmZCE = StatsCollector<>::registerNewStat("Cancel ZCE",
                                                         200000000, 400000000);
    newConfirmCFFEX = StatsCollector<>::registerNewStat("New CFFEX", 3000000,
                                                        6000000);
    cancelConfirmCFFEX = StatsCollector<>::registerNewStat("Cancel CFFEX",
                                                           3000000, 6000000);

    rmscheck = StatsCollector<>::registerNewStat("RMS", 2000, 5000);
    crosscheck = StatsCollector<>::registerNewStat("checkCross", 300, 500);
    orsdecode = StatsCollector<>::registerNewStat("MsgDecode", 2000, 10000);
    apiLatency = StatsCollector<>::registerNewStat("ApiLatency", 2000, 20000);
    respEnqueue = StatsCollector<>::registerNewStat("ResponseEnqueue", 1000,
                                                    20000);
}

ORSServer::ORSServer(::ors::OrsConfig config, ::ors::OrsTransport& transport)
    : config_(std::move(config)), transport_(transport) {
    m_iRequestID = 0;
    iNextOrderRef = 0;
    m_connected = false;

    m_rmsConfigFile = config_.rmsConfigFile;
    m_rmsFirmwideConfigFile = config_.rmsFirmwideConfigFile;

    registerStats();

    lastRecordTime = illuminati::ITime_ClockRT::GetCurrentTime();
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
    while (!m_connected)
    {
    }

    illuminati::sysinfo::ProcessSettings ps;
    ps.configure("LOGGER_THREAD");

    installSignals();
    HandleSignals();
}

void ORSServer::Stop()
{
    // accounts->Stop();
    m_connected = false;
    ILOG(INFO) << "Logging off\n";
    DoLoggOff();
    ILOG(INFO) << "Exiting\n";
    exit(0);
}

void ORSServer::RmsError(ResponseType resptype, uint32_t orderid,
                         uint32_t errorNo)
{
    ResponseMsg *resp = new ResponseMsg();
    resp->Response_Type = resptype;
    resp->OrderID = orderid;
    resp->ErrorCode = errorNo;
    resp->TimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    transport_.sendResponse(*resp);
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|"
               << "RMS error | orderID: " << orderid << " | errorNo: " << errorNo << std::endl;
    ILOG(INFO) << "|" << tempTimeStamp << "|" << resp->toString() << endl;
    delete resp;
}

void ORSServer::RmsWarning(ResponseType resptype, uint32_t orderid,
                           uint32_t errorNo)
{
    ResponseMsg *resp = new ResponseMsg();
    resp->Response_Type = resptype;
    resp->OrderID = orderid;
    resp->ErrorCode = errorNo;
    resp->TimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    transport_.sendResponse(*resp);
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "RMS Warning | orderID: " << orderid << " | errorNo: " << errorNo << std::endl;
    ILOG(INFO) << "|" << tempTimeStamp << "|" << resp->toString() << endl;
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
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Logout sent" << std::endl;
        // while (!m_logoffResp) {
        // }
    }
    else
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "|" << tempTimeStamp << "|"
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
        m_pUserTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(
            config_.flowDir.c_str());
    }
    if (m_pUserMDApi == nullptr) {
        m_pUserMDApi = CThostFtdcMdApi::CreateFtdcMdApi(
            config_.flowDir.c_str());
    }

    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|"
               << "Logging in to MD " << FRONT_ADDR_MD << "|" << m_BROKER_ID << "|" << m_INVESTOR_ID << "|" << m_PASSWORD << std::endl;
    // create MD classes
    m_pUserMDSpi = new CMdSpi(m_pUserMDApi);
    m_pUserMDApi->RegisterSpi(m_pUserMDSpi);
    m_pUserMDApi->RegisterFront((char *)FRONT_ADDR_MD.c_str());
    // m_pUserMDSpi->ReqUserLogin((char*)m_BROKER_ID.c_str(), (char*)m_INVESTOR_ID.c_str(), (char*)m_PASSWORD.c_str());
    m_pUserMDApi->Init();

    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|"
               << "Logging in to ORS " << FRONT_ADDR_ORS << "|" << m_BROKER_ID << "|" << m_INVESTOR_ID << "|" << m_PASSWORD << std::endl;
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
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    if (bIsLast && !IsErrorRspInfo(pRspInfo))
        ILOG(INFO) << "|" << tempTimeStamp << "|"
                   << "User logged out successfully" << std::endl;
    else
        ILOG(INFO) << "|" << tempTimeStamp << "|"
                   << "User logged out failure" << std::endl;
    m_logoffResp = true;
}

/******************************* SENDING ORDERS ***************************/
void ORSServer::ProcessRequest(RequestMsg *req)
{
    RequestMsgSerializer rs(*req);
    ILOGQ(MSG, rs);
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
    contractPosLock.lock();

    mapContractPosIter = mapContractPos.find(std::string(req.InstrumentID));

    if (mapContractPosIter == mapContractPos.end())
    {
        contractPos noPosition;
        noPosition.ONLongPos = 0;
        noPosition.todayLongPos = 0;
        noPosition.ONShortPos = 0;
        noPosition.todayShortPos = 0;

        mapContractPos[std::string(req.InstrumentID)] = noPosition;
        mapContractPosIter = mapContractPos.find(std::string(req.InstrumentID));
    }

    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    // ILOG(INFO) << "--->>> "<< tempTimeStamp << "|" << "Pos | " << req.InstrumentID << " | " <<  mapContractPosIter->second.ONLongPos << " | " <<  mapContractPosIter->second.todayLongPos << " | " << mapContractPosIter->second.ONShortPos << " | " << mapContractPosIter->second.todayShortPos << std::endl;

    switch (request->Transaction_Type)
    {
    case BUY:
        if (request->Quantity <= mapContractPosIter->second.todayShortPos)
        {
            if (request->Exchange_Type == CHINA_SHFE)
            {
                flag = THOST_FTDC_OF_CloseToday;
            }
            else
            {
                flag = THOST_FTDC_OF_Close;
            }
            mapContractPosIter->second.todayShortPos -= request->Quantity;
            orderType = CLOSE_TODAY;
        }
        else if (request->Quantity <= mapContractPosIter->second.ONShortPos)
        {
            flag = THOST_FTDC_OF_Close;
            mapContractPosIter->second.ONShortPos -= request->Quantity;
            orderType = CLOSE_YESTERDAY;
        }
        else
        {
            flag = THOST_FTDC_OF_Open;
            //				mapContractPosIter->second.todayLongPos += request->Quantity;
            orderType = OPEN_ORDER;
        }
        break;
    case SELL:
        if (request->Quantity <= mapContractPosIter->second.todayLongPos)
        {
            if (request->Exchange_Type == CHINA_SHFE)
            {
                flag = THOST_FTDC_OF_CloseToday;
            }
            else
            {
                flag = THOST_FTDC_OF_Close;
            }
            mapContractPosIter->second.todayLongPos -= request->Quantity;
            orderType = CLOSE_TODAY;
        }
        else if (request->Quantity <= mapContractPosIter->second.ONLongPos)
        {
            flag = THOST_FTDC_OF_Close;
            mapContractPosIter->second.ONLongPos -= request->Quantity;
            orderType = CLOSE_YESTERDAY;
        }
        else
        {
            flag = THOST_FTDC_OF_Open;
            //                               mapContractPosIter->second.todayShortPos += request->Quantity;
            orderType = OPEN_ORDER;
        }
        break;
    }
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    // ILOG(INFO) << "--->>> "<< tempTimeStamp << "|" << "Pos | " << req.InstrumentID << " | " <<  mapContractPosIter->second.ONLongPos << " | " <<  mapContractPosIter->second.todayLongPos << " | " << mapContractPosIter->second.ONShortPos << " | " << mapContractPosIter->second.todayShortPos << std::endl;
    contractPosLock.unlock();
}

void ORSServer::SendNewOrder(RequestMsg *request)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "NewOrder ------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << request->OrderID << std::endl;
    int iResult;
    CThostFtdcInputOrderField req;
    OrderInfo ordinfo;

    uint64_t rmstime, ordercrosscheck, apisend;
    uint64_t curtime = illuminati::ITime_ClockRT::GetCurrentTime();
    memset(&req, 0, sizeof(req));

    if (!rms.executeNewOrderChecks(*request))
    {
        ILOG(INFO) << "NewOrder ------> "
                   << "NewOrder check failed. orderID:" << request->OrderID << std::endl;
        return;
    }
    rmstime = illuminati::ITime_ClockRT::GetCurrentTime();

    ordercrosscheck = illuminati::ITime_ClockRT::GetCurrentTime();

    if (request->OrdType == MARKET)
    {

        // defaults:
        req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
        req.LimitPrice = 0;
        req.TimeCondition = THOST_FTDC_TC_IOC;
        req.VolumeTotalOriginal = request->Quantity;
        req.VolumeCondition = THOST_FTDC_VC_CV;
        req.MinVolume = request->Quantity;
        if ((request->Duration == illuminati::infra::FOK) || (request->Duration == illuminati::infra::IOC))
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
        else if (request->Duration == illuminati::infra::FAK)
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

        if ((request->Duration == illuminati::infra::FOK) || (request->Duration == illuminati::infra::IOC))
        {
            if ((request->Exchange_Type == CHINA_CFFEX) || (request->Exchange_Type == CHINA_ZCE))
            {
                RejectOrder(request, "Order type not supported in this market");
                return;
            }

            req.VolumeCondition = THOST_FTDC_VC_CV;
            req.MinVolume = request->Quantity;
        }
        else if (request->Duration == illuminati::infra::FAK)
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
    apisend = illuminati::ITime_ClockRT::GetCurrentTime();
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

    uint64_t endtime = illuminati::ITime_ClockRT::GetCurrentTime();

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
    ordinfo.qtyType = iOrderType;

    m_ordLocalMapLock.lock();
    m_ordLocalMap[request->OrderID] = ordinfo;
    m_ordLocalMapLock.unlock();

    m_ordExchMapLock.lock();
    m_ordExchMap[std::string(req.OrderRef)] = request->OrderID;
    m_ordExchMapLock.unlock();

    // StatsCollector<>::record( shmread, ((int64_t)curtime - (int64_t)request->TimeStamp)) ;
    StatsCollector<>::record(orsLatency,
                             ((int64_t)apisend - (int64_t)ordercrosscheck));
    StatsCollector<>::record(apiLatency,
                             ((int64_t)endtime - (int64_t)apisend));
    StatsCollector<>::record(rmscheck, ((int64_t)rmstime - (int64_t)curtime));
    StatsCollector<>::record(crosscheck,
                             ((int64_t)ordercrosscheck - (int64_t)rmstime));
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    //  ILOG(INFO) << "--->>> "<< tempTimeStamp << "|" << "New Order placed | " << request->Contract_Description.Symbol << " " << request->OrderID  << " " << ORDER_REF << " " << request->Price << " " << request->Quantity << std::endl ;
    // ILOG(INFO) << "------> "<< tempTimeStamp << "|" << __FUNCTION__  << " end OrderRef"<< request->OrderID << std::endl;
}

void ORSServer::SendModifyOrder(RequestMsg *request)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    RejectOrder(request, "Modify orders not allowed");
    return;
}

void ORSServer::SendCancelOrder(RequestMsg *request)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << request->OrderID << std::endl;
    CThostFtdcInputOrderActionField req;
    OrderInfo ordinfo;
    int iResult;
    uint64_t rmstime, apisend;
    uint64_t curtime = illuminati::ITime_ClockRT::GetCurrentTime();

    if (!rms.executeCancelOrderChecks(*request))
    {
        return;
    }
    rmstime = illuminati::ITime_ClockRT::GetCurrentTime();

    memset(&req, 0, sizeof(req));

    m_ordLocalMapLock.lock();
    if (m_ordLocalMap.find(request->OrderID) == m_ordLocalMap.end())
    {
        m_ordLocalMapLock.unlock();
        RejectOrder(request, "New not found for Cancel request");
        return;
    }
    ordinfo = m_ordLocalMap[request->OrderID];
    m_ordLocalMapLock.unlock();

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

    apisend = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "After API call 4\n";
    iResult = m_pUserTradeApi->ReqOrderAction(&req, ++m_iRequestID);
    ILOG(INFO) << "After API call 4\n";
    if (iResult != 0)
    {
        RejectOrder(request, "Cancel order send error");
        return;
    }
    uint64_t endtime = illuminati::ITime_ClockRT::GetCurrentTime();

    ordinfo.ModQtySent = 0;
    ordinfo.requestType = CANCELORDER; // request->Request_Type;
    ordinfo.sendTime = endtime;

    m_ordLocalMapLock.lock();
    m_ordLocalMap[request->OrderID] = ordinfo;
    m_ordLocalMapLock.unlock();

    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
               << "Cancel order sent | " << ordinfo.localID << " | " << ordinfo.exchID << '|' << request->Price << '|' << request->Quantity << std::endl;
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << request->OrderID << std::endl;

    // StatsCollector<>::record( shmread, ((int64_t)curtime - (int64_t)request->TimeStamp)) ;
    StatsCollector<>::record(orsLatency,
                             ((int64_t)apisend - (int64_t)rmstime));
    StatsCollector<>::record(apiLatency,
                             ((int64_t)endtime - (int64_t)apisend));
    StatsCollector<>::record(rmscheck, ((int64_t)rmstime - (int64_t)curtime));
}

void ORSServer::logOrderInfoResponse(const char *s, OrderInfo &ordinfo,
                                     ResponseMsg *resp)
{
    ILOG(INFO) << "|" << tempTimeStamp << "|" << s << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.OrderSysID << " | " << ordinfo.ScripCode << " | " << ordinfo.Price << " | " << ordinfo.Qty << " | " << (resp->TimeStamp - ordinfo.sendTime) << std::endl;
}

void ORSServer::PushUpdate(ResponseMsg *resp)
{
    resp->TimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    // m_responseInterface->lock();
    transport_.sendResponse(*resp);
    StatsCollector<>::record(respEnqueue,
                             illuminati::ITime_ClockRT::GetCurrentTime() - resp->TimeStamp);
    // m_responseInterface->unlock();

    OrderInfo ordinfo;
    // if(m_ordLocalMap.find(resp->OrderID) != m_ordLocalMap.end())
    m_ordLocalMapLock.lock();
    ordinfo = m_ordLocalMap[resp->OrderID];
    m_ordLocalMapLock.unlock();
    if (ordinfo.sendTime && resp->Response_Type != TRADE_CONFIRM)
    {
        switch (resp->Response_Type)
        {
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        case NEW_ORDER_CONFIRM:
        case ORDER_ERROR:
            switch (ordinfo.exchange)
            {
            case CHINA_SHFE:
                logOrderInfoResponse("NC | SHFE", ordinfo, resp);
                StatsCollector<>::record(newConfirmSHFE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_CFFEX:
                logOrderInfoResponse("NC | CFFEX", ordinfo, resp);
                StatsCollector<>::record(newConfirmCFFEX,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_ZCE:
                logOrderInfoResponse("NC | ZCE", ordinfo, resp);
                StatsCollector<>::record(newConfirmZCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_DCE:
                logOrderInfoResponse("NC | DCE", ordinfo, resp);
                StatsCollector<>::record(newConfirmDCE,
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
                StatsCollector<>::record(cancelConfirmSHFE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_CFFEX:
                logOrderInfoResponse("CC | CFFEX", ordinfo, resp);
                StatsCollector<>::record(cancelConfirmCFFEX,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_ZCE:
                logOrderInfoResponse("CC | ZCE", ordinfo, resp);
                StatsCollector<>::record(cancelConfirmZCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            case CHINA_DCE:
                logOrderInfoResponse("CC | DCE", ordinfo, resp);
                StatsCollector<>::record(cancelConfirmDCE,
                                         resp->TimeStamp - ordinfo.sendTime);
                break;
            }
            break;
        }
    }

    // update RMS here
    rms.handleORSResponse(resp);

    // to update Position map
    //
    updatePosition(ordinfo, resp);
}

void ORSServer::updatePosition(OrderInfo &ordinfo, ResponseMsg *resp)
{
    contractPosLock.lock();
    mapContractPosIter = mapContractPos.find(std::string(ordinfo.ScripCode));
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    // ILOG(INFO) << "|"<< tempTimeStamp << "|" << "Pos | " << ordinfo.ScripCode << " | " <<  mapContractPosIter->second.ONLongPos << " | " <<  mapContractPosIter->second.todayLongPos << " | " << mapContractPosIter->second.ONShortPos << " | " << mapContractPosIter->second.todayShortPos << std::endl;
    switch (resp->Response_Type)
    {
    case ORDER_ERROR:
        ILOG(INFO) << "Currrent position: " << mapContractPosIter->second.todayLongPos << " " << mapContractPosIter->second.todayShortPos << " " << mapContractPosIter->second.ONShortPos << " " << mapContractPosIter->second.ONLongPos << "\n";
        switch (ordinfo.qtyType)
        {
        case OPEN_ORDER:
            // Do nothing
            break;
        case CLOSE_TODAY:
            if (ordinfo.Transaction_Type == BUY)
            {
                mapContractPosIter->second.todayShortPos += ordinfo.Qty;
            }
            else
            {
                mapContractPosIter->second.todayLongPos += ordinfo.Qty;
            }
            break;
        case CLOSE_YESTERDAY:
            if (ordinfo.Transaction_Type == BUY)
            {
                mapContractPosIter->second.ONShortPos += ordinfo.Qty;
            }
            else
            {
                mapContractPosIter->second.ONLongPos += ordinfo.Qty;
            }
            break;
        }
        break;
    case CANCEL_ORDER_CONFIRM:
        switch (ordinfo.qtyType)
        {
        //				case OPEN_ORDER:
        //				if(ordinfo.Transaction_Type == BUY)
        //				{
        //					mapContractPosIter->second.todayLongPos -= ordinfo.openQty;
        //				}
        //				else
        //				{
        //					mapContractPosIter->second.todayShortPos -= ordinfo.openQty;
        //				}
        //				break;
        case CLOSE_TODAY:
            if (ordinfo.Transaction_Type == BUY)
            {
                mapContractPosIter->second.todayShortPos += resp->Quantity; // ordinfo.openQty;
            }
            else
            {
                mapContractPosIter->second.todayLongPos += resp->Quantity; // ordinfo.openQty;
            }
            break;
        case CLOSE_YESTERDAY:
            if (ordinfo.Transaction_Type == BUY)
            {
                mapContractPosIter->second.ONShortPos += resp->Quantity; // ordinfo.openQty;
            }
            else
            {
                mapContractPosIter->second.ONLongPos += resp->Quantity; // ordinfo.openQty;
            }
            break;
        }
        break;
    case TRADE_CONFIRM:
        switch (ordinfo.qtyType)
        {
        case OPEN_ORDER:
            if (ordinfo.Transaction_Type == BUY)
            {
                mapContractPosIter->second.todayLongPos += resp->Quantity;
            }
            else
            {
                mapContractPosIter->second.todayShortPos += resp->Quantity;
            }
            break;
        case CLOSE_TODAY:
        case CLOSE_YESTERDAY:
            // dont update here since while sending we have already updated the maps
            break;
        }
        break;
    }
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    // ILOG(INFO) << "|"<< tempTimeStamp << "|" << "Pos | " << ordinfo.ScripCode << " | " <<  mapContractPosIter->second.ONLongPos << " | " <<  mapContractPosIter->second.todayLongPos << " | " << mapContractPosIter->second.ONShortPos << " | " << mapContractPosIter->second.todayShortPos << std::endl;
    contractPosLock.unlock();
}

void ORSServer::RejectOrder(RequestMsg *req, const char *Reason)
{
    ResponseMsg response;
    if (req->Request_Type == NEWORDER)
    {
        // response->Response_Type = ORDER_ERROR ;
        response.Response_Type = ORS_REJECT;
        response.ErrorCode = ERROR_CODE_CROSS_REJECT;
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "New Order rejected | " << req->OrderID << " " << Reason << std::endl;
    }
    if (req->Request_Type == MODIFYORDER)
    {
        response.Response_Type = MODIFY_ORDER_REJECT;
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Modify Order rejected | " << req->OrderID << " " << Reason << std::endl;
    }
    if (req->Request_Type == CANCELORDER)
    {
        response.Response_Type = CANCEL_ORDER_REJECT;
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << "InstrumentID : " << pInstrumentStatus->InstrumentID << " ExchangeID : " << pInstrumentStatus->ExchangeID << " InstrumentStatus : " << pInstrumentStatus->InstrumentStatus << std::endl;
    if (pInstrumentStatus == NULL)
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
        
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << data.ScripCode << "|" << data.RoundLot << "|" << data.DecimalLocator << "|" << data.ExpiryDate << "|" << data.ExchangeID << "|" << data.PriceTick << "|" << data.IsTrading << std::endl;
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
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
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
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    if (!iResult)
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument info query successful" << std::endl;
    else
    {
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument info query failed... exiting..." << std::endl;
        exit(1);
    }
}

void ORSServer::ReqQryPosition()
{
    return; // modify by pqr 20230213
}

void ORSServer::ReqQryAccount()
{
    return; // modify by pqr 20230213
}

void ORSServer::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    MasterData data;
    if (pInstrument == NULL)
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "First param as NULL OnRspQryInstrument" << std::endl;
        return;
    }
    if (!IsErrorRspInfo(pRspInfo))
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            
                tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
                ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << data.ScripCode << "|" << data.RoundLot << "|" << data.DecimalLocator << "|" << data.ExpiryDate << "|" << data.ExchangeID << "|" << data.PriceTick << "|" << data.IsTrading << std::endl;
            m_symbolMap[std::string(pInstrument->InstrumentID)] = data;
        }
    }
    else
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "OnRspQryInstrument error on callback" << std::endl;
    }
    if (bIsLast)
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Instrument query complete" << std::endl;
        m_pUserMDSpi->SubscribeMarketData();
        rms.setExchangeInstrDefinitionMap(m_symbolMap, true, true, false);
    }
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

    ILOG(INFO) << "|ctp auth response:[" << ret << "], waiting for auth push" << std::endl;
    std::cout << "ctp auth response:[" << ret << "], waiting for auth push" << endl;
}

void ORSServer::ReqUserLogin()
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << std::endl;
    CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.BrokerID, m_BROKER_ID.c_str());
    strcpy(req.UserID, m_INVESTOR_ID.c_str());
    strcpy(req.Password, m_PASSWORD.c_str());
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|"
               << "BrokerID " << req.BrokerID << " UserID " << req.UserID << " Password " << req.Password << std::endl;
    m_logonResp = false;
    int iResult = m_pUserTradeApi->ReqUserLogin(&req, ++m_iRequestID);
    if (!iResult)
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Login send success" << std::endl;
    }
    else
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Login send failure" << std::endl;
        exit(0);
    }
    // ReqUserLogin(req.BrokerID, req.UserID, req.Password);
}

void ORSServer::OnFrontConnected()
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|"
               << "OnFrontConnected ORS" << std::endl;
    Authenticate();
    // ReqUserLogin();
}

void ORSServer::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "|" << tempTimeStamp << "|"
               << "OnRspSettlementInfoConfirm" << std::endl;
    if (bIsLast && !IsErrorRspInfo(pRspInfo))
    {
        ReqQryInstrument(NULL);
    }
}

bool ORSServer::OrderCrossCheck(RequestMsg *request)
{
    OrderAccountMapIter openorders;
    m_ordLocalMapLock.lock();
    for (openorders = m_ordLocalMap.begin(); openorders != m_ordLocalMap.end();
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
                    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
                    ILOG(INFO) << "|" << tempTimeStamp << "|"
                               << "Cross : " << openorders->first << " | " << request->OrderID << std::endl;
                    m_ordLocalMapLock.unlock();
                    return true;
                }
            }
        }
    }
    m_ordLocalMapLock.unlock();
    return false;
}

/*
 {
 std::cout << __FUNCTION__ << std::endl;
 CSgitFtdcInputOrderActionField req;

 OrderInfo ordinfo;
 int iResult;
 uint64_t rmstime, apisend;
 uint64_t curtime = illuminati::ITime_ClockRT::GetCurrentTime() ;
 /*
 if (!rms.executeCancelOrderChecks(*request))
 {
 return;
 }
 *
 rmstime = illuminati::ITime_ClockRT::GetCurrentTime();
 CSgitFtdcInputOrderActionField reqOrderAction = { 0 };

 if(m_ordLocalMap.find(request->OrderID) == m_ordLocalMap.end())
 {
 RejectOrder( request, "New not found for Cancel request\n") ;
 return;
 }
 ordinfo = m_ordLocalMap[request->OrderID];
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

 apisend = illuminati::ITime_ClockRT::GetCurrentTime() ;
 iResult = m_pUserTradeApi->ReqOrderAction(&reqOrderAction, ++m_iRequestID);
 if (iResult != 0)
 {
 RejectOrder( request, "Cancel order send error\n") ;
 return ;
 }
 uint64_t endtime = illuminati::ITime_ClockRT::GetCurrentTime() ;

 ordinfo.ModQtySent = 0;
 ordinfo.requestType = request->Request_Type;
 ordinfo.sendTime = endtime;
 //ordinfo.orderSent = 1;
 m_ordLocalMap[request->OrderID] = ordinfo;
 m_ordExchMap[std::string(req.OrderRef)] = ordinfo;

 std::cout << "Cancel order sent | " << ordinfo.localID << " | " << ordinfo.exchID << '|' << request->Price << '|' << request->Quantity << std::endl ;

 //StatsCollector<>::record( shmread, ((int64_t)curtime - (int64_t)request->TimeStamp)) ;
 StatsCollector<>::record( orsLatency, ((int64_t)apisend - (int64_t)rmstime)) ;
 StatsCollector<>::record( apiLatency, ((int64_t)endtime - (int64_t)apisend)) ;
 StatsCollector<>::record( rmscheck, ((int64_t)rmstime - (int64_t)curtime)) ;
 }
 */

bool ORSServer::GetOrderInfo(const char *orderref, OrderInfo &orderinfo)
{
    // TODO : temporary solution.. can do better
    uint32_t oid;
    m_ordExchMapLock.lock();
    auto localID = m_ordExchMap.find(std::string(orderref));
    if (localID == m_ordExchMap.end())
    {
        ILOG(FATAL) << "OrderRef: " << orderref << " not found in map\n";
        m_ordExchMapLock.unlock();
        return false;
    }
    else
    {
        oid = localID->second;
    }
    m_ordExchMapLock.unlock();

    m_ordLocalMapLock.lock();

    auto oinfo_it = m_ordLocalMap.find(oid);
    if (oinfo_it == m_ordLocalMap.end())
    {
        ILOG(FATAL) << "OrderID: " << oid << " not found in map\n";
        m_ordLocalMapLock.unlock();
        return false;
    }
    else
    {
        orderinfo = oinfo_it->second;
    }

    m_ordLocalMapLock.unlock();
    return true;
}

void ORSServer::RemoveFromMap(const char *exchid, int32_t localid)
{
    m_ordLocalMapLock.lock();
    OrderAccountMapIter iter = m_ordLocalMap.find(localid);
    if (iter != m_ordLocalMap.end())
    {
        m_ordLocalMap.erase(iter);
    }
    m_ordLocalMapLock.unlock();

    m_ordExchMapLock.lock();
    OrderAccountMapIter2 iter2 = m_ordExchMap.find(std::string(exchid));
    if (iter2 != m_ordExchMap.end())
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        //   ILOG(INFO) << "--->>> "<< tempTimeStamp << "|" << "Erased order from map " << localid << " | " << exchid << std::endl;
        m_ordExchMap.erase(iter2);
    }
    m_ordExchMapLock.unlock();
}


void ORSServer::OnRspOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pOrderAction->OrderRef << std::endl;
    bool bRemoveOrder = false;
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << "OrderSysID : " << pOrderAction->OrderRef << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrderAction->OrderRef << std::endl;
}

void ORSServer::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef " << pInputOrder->OrderRef << " OrderSysID : " << pInputOrder->OrderRef << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    bool bRemoveOrder = false;
    // Changed since error code 0 is also valid in this case
    // if(pRspInfo->ErrorID != 0)
    {
        ResponseMsg response;
        OrderInfo ordinfo;
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = false;
            break;
        default:
            response.Response_Type = ORDER_ERROR;
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            m_ordLocalMapLock.lock();
            m_ordLocalMap[response.OrderID] = ordinfo;
            m_ordLocalMapLock.unlock();
        }
    }
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pInputOrder->OrderRef << std::endl;
}

void ORSServer::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef " << pOrderAction->OrderRef << " OrderSysID : " << pOrderAction->OrderSysID << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    bool bRemoveOrder = false;
    // Changed since error code 0 is also valid in this case
    // if(pRspInfo->ErrorID != 0)
    {
        ResponseMsg response;
        OrderInfo ordinfo;
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = false;
            break;
        default:
            response.Response_Type = ORDER_ERROR;
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            m_ordLocalMapLock.lock();
            m_ordLocalMap[response.OrderID] = ordinfo;
            m_ordLocalMapLock.unlock();
        }
    }
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrderAction->OrderRef << std::endl;
}

void ORSServer::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pInputOrder->OrderRef << std::endl;
    bool bRemoveOrder = false;
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|" << __FUNCTION__ << " OrderSysID : " << pInputOrder->OrderRef << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    // Changed since error code 0 is also valid in this case
    // if(pRspInfo->ErrorID != 0)
    {
        ResponseMsg response;
        OrderInfo ordinfo;

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
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        switch (ordinfo.requestType)
        {
        case NEWORDER:
            response.Response_Type = ORDER_ERROR;
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        default:
            response.Response_Type = ORDER_ERROR;
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            m_ordLocalMapLock.lock();
            m_ordLocalMap[response.OrderID] = ordinfo;
            m_ordLocalMapLock.unlock();
        }
    }
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pInputOrder->OrderRef << std::endl;
}

void ORSServer::OnRtnOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pOrder->OrderRef << __FUNCTION__ << "OrderSysID : " << pOrder->OrderSysID << " ErrorID : " << pRspInfo->ErrorID << " ErrorMsg : " << pRspInfo->ErrorMsg << std::endl;
    if (pRspInfo->ErrorID != 0 || pOrder->OrderSysID == NULL)
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Order error, errorcode=" << pRspInfo->ErrorID << " errormsg= " << pRspInfo->ErrorMsg << std::endl;
        pOrder->OrderSubmitStatus = THOST_FTDC_OSS_InsertRejected - '0';
    }
    OnRtnOrder(pOrder);
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrder->OrderRef << std::endl;
}

void ORSServer::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef" << pOrder->OrderRef << std::endl;
    OrderInfo ordinfo;
    ResponseMsg response;
    bool bRemoveOrder = false;

    if (!(GetOrderInfo(pOrder->OrderRef, ordinfo)))
    {
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel confirm | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << " | " << response.Quantity << std::endl;
            if (ordinfo.Qty == 0)
            {
                bRemoveOrder = true;
            }
            else
            {
                tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
                ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                           << "Partial cancel condition triggered2" << std::endl;
            }
            break;
        case THOST_FTDC_OST_Unknown:
            ordinfo.Qty = pOrder->VolumeTotalOriginal;
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Request submitted | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        /*
         case THOST_FTDC_OST_NotTouched:
         break;
         case THOST_FTDC_OST_Touched:
         break;
         */
        default:
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "LocalID | " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "unhandled order status | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        }
        break;
    case THOST_FTDC_OSS_Accepted:
        switch (pOrder->OrderStatus)
        {
        case THOST_FTDC_OST_AllTraded:
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
                tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
                ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel confirm | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            if (ordinfo.Qty == 0)
            {
                bRemoveOrder = true;
            }
            else
            {
                tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
                ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "LocalID | " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "New order error | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            bRemoveOrder = true;
            break;
        case CANCELORDER:
            response.Response_Type = CANCEL_ORDER_REJECT;
            PushUpdate(&response);
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "Cancel reject | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << ordinfo.Price << " | " << ordinfo.Qty << std::endl;
            break;
        default:
            // ordinfo.Qty = pOrder->VolumeTotalOriginal;
            response.Response_Type = ORDER_ERROR;
            PushUpdate(&response);
            tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                       << "LocalID | " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
            ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "LocalID " << ordinfo.localID << " OrderSysID " << pOrder->OrderSysID << " OrderSubmitStatus " << pOrder->OrderSubmitStatus << " OrderStatus " << pOrder->OrderStatus << std::endl;
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
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
        m_ordLocalMapLock.lock();
        m_ordLocalMap[response.OrderID] = ordinfo;
        m_ordLocalMapLock.unlock();
    }
    // UpdateRMS(res, ordinfo.ScripCode, ordinfo.ClientID, ordinfo.Transaction_Type);
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef" << pOrder->OrderRef << std::endl;
}


void ORSServer::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
//	ILOG(INFO) << "------> "<< tempTimeStamp << "|" << __FUNCTION__ << " start OrderRef " << pTrade->OrderRef << std::endl;
    OrderInfo ordinfo;
    ResponseMsg response;

    /*
     if (!GetOrderInfo(pTrade->OrderRef, ordinfo))
     return;

     ordinfo.Qty = ordinfo.Qty - pTrade->Volume;
     ordinfo.openQty = ordinfo.Qty;

     if (ordinfo.openQty > 0) {
     m_ordLocalMapLock.lock();
     } else {
     RemoveFromMap(ordinfo.exchID, ordinfo.localID);
     }
     */
    // TODO: Didn't use GetOrderInfo directly because there was some unusal locking in this function
    uint32_t oid;
    m_ordExchMapLock.lock();
    auto localID_it = m_ordExchMap.find(std::string(pTrade->OrderRef));
    if (localID_it == m_ordExchMap.end())
    {
        ILOG(FATAL) << "OrderRef: " << pTrade->OrderRef << " not found in map\n";
        m_ordExchMapLock.unlock();
        return;
    }
    else
    {
        oid = localID_it->second;
    }
    m_ordExchMapLock.unlock();

    m_ordLocalMapLock.lock();
    ordinfo = m_ordLocalMap[oid];
    ordinfo.Qty = ordinfo.Qty - pTrade->Volume;
    ordinfo.openQty = ordinfo.Qty;
    m_ordLocalMap[oid] = ordinfo;
    m_ordLocalMapLock.unlock();

    response.Response_Type = TRADE_CONFIRM;
    response.OrderID = ordinfo.localID;
    response.Price = pTrade->Price;
    response.Quantity = pTrade->Volume;
    strcpy(response.AccountID, ordinfo.ClientID);
    strcpy(response.Symbol, ordinfo.ScripCode);
    response.Side = ordinfo.Transaction_Type;

    PushUpdate(&response);

    ordinfo.orderSent = 0;

    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
               << "Trade | " << ordinfo.ClientID << " | " << ordinfo.ScripCode << " | " << ordinfo.localID << " | " << ordinfo.exchID << " | " << ordinfo.Transaction_Type << " | " << pTrade->Price << " | " << pTrade->Volume << " | " << ordinfo.openQty << std::endl;

    // UpdateRMS(res, ordinfo.ScripCode, ordinfo.ClientID, ordinfo.Transaction_Type);

    // Now this should be done by order response only
    if (ordinfo.openQty > 0)
    {
        tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
        ILOG(INFO) << "--->>> " << tempTimeStamp << "|"
                   << "Partially traded" << std::endl;
        // m_ordLocalMap[response.OrderID] = ordinfo;
        // m_ordExchMap[std::string(ordinfo.exchID)] = ordinfo;
    }
    else
    {
        RemoveFromMap(ordinfo.exchID, response.OrderID);
    }
    tempTimeStamp = illuminati::ITime_ClockRT::GetCurrentTime();
    ILOG(INFO) << "------> " << tempTimeStamp << "|" << __FUNCTION__ << " end OrderRef " << pTrade->OrderRef << std::endl;
}

void ORSServer::installSignals()
{
    ILOG(INFO) << "Installing signals\n";
    sigfillset(&m_signalSet);
    sigaddset(&m_signalSet, SIGUSR1);
    sigaddset(&m_signalSet, SIGUSR2);
    sigaddset(&m_signalSet, SIGTSTP);
    sigaddset(&m_signalSet, SIGINT);
    sigaddset(&m_signalSet, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &m_signalSet, NULL) != 0)
    {
        ILOG(FATAL) << "sigmask error: " << strerror(errno) << std::endl;
        throw("sigmask error");
    }
}

void ORSServer::HandleSignals()
{

    illuminati::sysinfo::ProcessSettings ps;

    int32_t sig = 0;
    while (true)
    {
        ILOG(INFO) << "Signal handler thread sleeping\n";
        if (sigwait(&m_signalSet, &sig) != 0)
        {
            ILOG(FATAL) << "Sigwait error " << std::endl;
        }

        switch (sig)
        {
        case SIGTSTP: // ctrl+z
        {
            ILOG(INFO) << "Attempting ORS normal exit." << std::endl;
            ILOG(INFO) << "Shutting down the SHM manager \n";
            Stop();
            break;
        }
        case SIGTERM: // default by kill command
        {
            ILOG(INFO) << "Attempting ORS normal exit." << std::endl;
            ILOG(INFO) << "Shutting down the SHM manager \n";
            Stop();
            break;
        }
        case SIGINT: // ctrl+c
        {
            ILOG(INFO) << "ORS  exiting." << std::endl;
            Stop();
            break;
        }
        case SIGUSR1:
        {
            ILOG(INFO) << "Resetting RMS state." << std::endl;
            rms.reset();
            break;
        }
        case SIGUSR2:
        {
            ILOG(INFO) << "Fetch positions " << std::endl;
            ReqQryPosition();
            break;
        }

        default:
        {

            ILOG(INFO) << "Received Unhandled signal: " << sig << std::endl;
        }
        }
    }
}

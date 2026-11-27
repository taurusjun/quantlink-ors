#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ThostFtdcTraderApi.h"
#include "MdSpi.h"
#include "MDHandler.h"
#include "orderresponse.h"
#include "marketupdateNew.h"
#include <signal.h>
#include "errorcodes.h"

#include "rmsimpl.hpp"

#include "core/OrsConfig.h"
#include "core/OrsTypes.h"
#include "core/PositionTable.h"
#include "control/ClientRegistry.h"
#include "query/AccountQueryState.h"
#include "transport/OrsTransport.h"


typedef struct _masterData
{
    char ScripCode[31];
    unsigned long RoundLot;
    unsigned long DecimalLocator;
    double Factor;
    long DPRHigh;
    long DPRLow;
    unsigned long Price;
    char ExpiryDate[9];
    char InstrumentCode[21];
    char OptionType[3];
    int StrikePrice;
    char ExchangeID[31];
    int MaxMarketOrderVolume;
    int MinMarketOrderVolume;
    int MaxLimitOrderVolume;
    int MinLimitOrderVolume;
    double PriceTick;
    bool IsTrading;
    // can be used to set falg while sending new order
    int NetPositon;
} MasterData;

typedef std::unordered_map<std::string, MasterData> SymbolMap;
typedef std::unordered_map<std::string, MasterData>::iterator SymbolMapIter;

typedef struct _orderInfo
{
    ::ors::infra::RequestType requestType;
    bool orderSent;
    unsigned long localID;
    char exchID[31];
    char OrderSysID[21];
    char ClientID[11];
    char ScripCode[31];
    unsigned char Transaction_Type;
    // just for RMS
    double Price;
    unsigned long Qty;
    double oldPrice;
    unsigned long oldQty;
    unsigned long ModQtySent;
    // to keep track if the order is completely traded or open
    unsigned long openQty;
    std::uint64_t sendTime;
    int frontId;
    int sessionId;
    char exchange;
    int qtyType; // is this open or close or closeyesterday
} OrderInfo;

// local order id, order info
typedef std::map<unsigned long, OrderInfo> OrderAccountMap;
typedef std::map<unsigned long, OrderInfo>::iterator OrderAccountMapIter;

typedef std::map<std::string, std::uint32_t> OrderAccountMap2;
typedef std::map<std::string, std::uint32_t>::iterator OrderAccountMapIter2;

class ORSServer : public CThostFtdcTraderSpi
{
public:
    ORSServer(::ors::OrsConfig config, ::ors::OrsTransport& transport);
    ~ORSServer();
    void ProcessRequest(::ors::infra::RequestMsg *);
    void Login();
    void Start();
    void Stop();
    void RegisterClient(const ::ors::ClientRegisterRequest&);
    void QueryAccount(const ::ors::AccountQueryRequest&);
    void RejectOrder(::ors::infra::RequestMsg *, const char *);
    inline __attribute__((always_inline)) void PushUpdate(::ors::infra::ResponseMsg *);
    inline __attribute__((always_inline)) bool OrderCrossCheck(::ors::infra::RequestMsg *);
    void RmsError(::ors::infra::ResponseType, std::uint32_t, std::uint32_t);
    void RmsWarning(::ors::infra::ResponseType, std::uint32_t, std::uint32_t);

    ::ors::infra::RmsImpl &getRms()
    {
        return rms;
    }

    SymbolMap m_symbolMap;
    OrderAccountMap m_ordLocalMap;
    OrderAccountMap2 m_ordExchMap;
    std::mutex m_ordExchMapLock;
    volatile bool m_connected;
    bool m_logReqResp;
    std::thread m_reqrespLogger;
    int m_iRequestID;
    std::string m_BROKER_ID;
    std::string m_INVESTOR_ID;
    std::string m_INVESTOR_ID_SHFE;
    std::string m_INVESTOR_ID_CFFEX;
    std::string m_INVESTOR_ID_DCE;
    std::string m_INVESTOR_ID_ZCE;
    std::string m_PASSWORD;
    std::string m_NEWPASSWORD;
    std::string m_AUTHCODE{""};
    std::string m_APPID{""};
    int iNextOrderRef;

    void logOrderInfoResponse(const char *, OrderInfo &ordinfo,
                              ::ors::infra::ResponseMsg *resp);

    CThostFtdcTraderApi *m_pUserTradeApi;
    CThostFtdcMdApi *m_pUserMDApi;
    CMdSpi *m_pUserMDSpi;

    virtual void OnFrontConnected();
    virtual void OnFrontDisconnected(int nReason);
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspUserLogout(CThostFtdcUserLogoutField *, CThostFtdcRspInfoField *, int, bool);
    virtual void OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus);
    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnHeartBeatWarning(int nTimeLapse);
    // virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    // virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    // virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);
    virtual void OnRtnOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo);
    virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspOrderAction(CThostFtdcOrderActionField *, CThostFtdcRspInfoField *, int, bool);
    virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);
    virtual void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);
    virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);


private:
    ::ors::OrsConfig config_;
    ::ors::OrsTransport& transport_;
    inline __attribute__((always_inline)) void RemoveFromMap(const char *exchid, std::int32_t localid);
    inline __attribute__((always_inline)) bool GetOrderInfo(const char *orderref, OrderInfo &orderinfo);

    void SetCombOffsetFlag(CThostFtdcInputOrderField &req,
                           ::ors::infra::RequestMsg *request,
                           char &flag, int &orderType);
    std::mutex m_ordLocalMapLock;
    void registerStats();
    inline __attribute__((always_inline)) void SendNewOrder(::ors::infra::RequestMsg *);
    inline __attribute__((always_inline)) void SendModifyOrder(::ors::infra::RequestMsg *);
    inline __attribute__((always_inline)) void SendCancelOrder(::ors::infra::RequestMsg *);
    void DoLoggOff();
    void updatePosition(OrderInfo &ordinfo, ::ors::infra::ResponseMsg *resp);

    void ReqUserLogin();
    void Authenticate(); // add by pqr 20230213
    void ReqSettlementInfoConfirm();
    void ReqQrySettlementInfo();
    void ReqQryInstrument(char *);
    void ReqQryPosition();
    void ReqQryOrder();
    void ReqQryAccount();
    void installSignals();
    void HandleSignals();
    bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);
    TThostFtdcFrontIDType FRONT_ID;
    TThostFtdcSessionIDType SESSION_ID;
    TThostFtdcOrderRefType ORDER_REF;

    int m_logoffResp;
    int m_logonResp;
    ::ors::infra::RmsImpl rms;

    ::ors::PositionTable positionTable_;
    ::ors::ClientRegistry clientRegistry_;
    ::ors::AccountQueryState accountQueryState_;
    std::thread accountQueryTimer_;
    bool clientRegistered_{false};
    bool rmsReady_{false};
    std::vector<::ors::PositionRow> pendingPositionRows_;
    std::vector<CThostFtdcOrderField> pendingStartupOrders_;
    bool startupOrderQueryIssued_{false};

    std::string m_rmsConfigFile;
    std::string m_rmsFirmwideConfigFile;
    int m_maxClients{250};

    int m_mdShmKey;
    int m_mdShmSize;

    // signals
    sigset_t m_signalSet;

    std::string m_statsFile;

    std::uint64_t tempTimeStamp{0};
};

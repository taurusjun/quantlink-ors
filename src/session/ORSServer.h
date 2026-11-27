#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ThostFtdcTraderApi.h"
#include "marketdata/MdSpi.h"
#include "marketdata/MDHandler.h"
#include "orderresponse.h"
#include "marketupdateNew.h"
#include <signal.h>
#include "errorcodes.h"

#include "rmsimpl.hpp"

#include "config/OrsConfig.h"
#include "domain/OrsTypes.h"
#include "domain/OrderInfo.h"
#include "execution/OrderStore.h"
#include "execution/RmsGate.h"
#include "execution/OrderService.h"
#include "domain/PositionTable.h"
#include "execution/ClientRegistry.h"
#include "query/AccountQueryService.h"
#include "query/StartupQuery.h"
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

class OrderService;

class ORSServer : public CThostFtdcTraderSpi
{
public:
    friend class ::ors::OrderService;
    ORSServer(::ors::OrsConfig config, ::ors::OrsTransport& transport,
              MDHandler& mdHandler,
              ::ors::md::MarketUpdateNew& marketUpdate);
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

    ::ors::OrderStore &orderStore() { return orders_; }
    const ::ors::OrderStore &orderStore() const { return orders_; }

    ::ors::infra::RmsImpl &getRms()
    {
        return rms_.impl();
    }

    SymbolMap m_symbolMap;
    ::ors::OrderStore orders_;
    std::atomic_bool m_connected{false};
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

    struct TraderApiDeleter {
        void operator()(CThostFtdcTraderApi* api) const noexcept {
            if (api != nullptr) api->Release();
        }
    };
    struct MdApiDeleter {
        void operator()(CThostFtdcMdApi* api) const noexcept {
            if (api != nullptr) api->Release();
        }
    };
    std::unique_ptr<CThostFtdcTraderApi, TraderApiDeleter> m_pUserTradeApi;
    std::unique_ptr<CThostFtdcMdApi, MdApiDeleter> m_pUserMDApi;
    std::unique_ptr<CMdSpi> m_pUserMDSpi;

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
    MDHandler& mdHandler_;
    ::ors::md::MarketUpdateNew& marketUpdate_;
    inline __attribute__((always_inline)) void RemoveFromMap(const char *exchid, std::int32_t localid);
    inline __attribute__((always_inline)) bool GetOrderInfo(const char *orderref, ::ors::OrderInfo &orderinfo);

    void SetCombOffsetFlag(CThostFtdcInputOrderField &req,
                           ::ors::infra::RequestMsg *request,
                           char &flag, int &orderType);
    void registerStats();
    inline __attribute__((always_inline)) void SendNewOrder(::ors::infra::RequestMsg *);
    inline __attribute__((always_inline)) void SendModifyOrder(::ors::infra::RequestMsg *);
    inline __attribute__((always_inline)) void SendCancelOrder(::ors::infra::RequestMsg *);
    void DoLoggOff();
    void updatePosition(::ors::OrderInfo &ordinfo, ::ors::infra::ResponseMsg *resp);

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

    int m_logoffResp;
    int m_logonResp;
    ::ors::RmsGate rms_;
    ::ors::OrderService orderService_;

    ::ors::PositionTable positionTable_;
    ::ors::ClientRegistry clientRegistry_;
    ::ors::AccountQueryService accountQueryService_;
    std::thread accountQueryTimer_;
    std::atomic_bool stopRequested_{false};
    std::mutex readyMutex_;
    std::condition_variable readyCv_;
    bool rmsReady_{false};
    std::unique_ptr<::ors::StartupQuery> startupQuery_;

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

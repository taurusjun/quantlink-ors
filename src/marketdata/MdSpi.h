#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>


#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "ThostFtdcMdApi.h"

class ORSServer;
class MDHandler;
namespace ors::md { struct MarketUpdateNew; }

class CMdSpi : public CThostFtdcMdSpi
{
public:
    CMdSpi(CThostFtdcMdApi *pUserApi, ORSServer& server,
           MDHandler& mdHandler, ors::md::MarketUpdateNew& marketUpdate)
        : m_pUserApi(pUserApi),
          server_(server),
          mdHandler_(mdHandler),
          marketUpdate_(marketUpdate) {}
    ~CMdSpi() {}
    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnFrontDisconnected(int nReason);
    virtual void OnHeartBeatWarning(int nTimeLapse);
    virtual void OnFrontConnected();
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);
    // virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void ReqUserLogin(TThostFtdcBrokerIDType BROKER_ID, TThostFtdcInvestorIDType INVESTOR_ID, TThostFtdcPasswordType PASSWORD);
    void SubscribeMarketData();
    std::atomic<int> m_iLoginMD{0};
    bool waitForLogin(std::chrono::milliseconds timeout);
    void markLogin(int value);

private:
    // void ReqUserLogin();
    bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);
    std::uint64_t tempTimeStamp{0};
    int requestId_{0};
    std::mutex loginMutex_;
    std::condition_variable loginCv_;
    CThostFtdcMdApi *m_pUserApi;
    ORSServer& server_;
    MDHandler& mdHandler_;
    ors::md::MarketUpdateNew& marketUpdate_;
};

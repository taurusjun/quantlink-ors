#ifndef _MDSPI_HEADER_
#define _MDSPI_HEADER_



#include "itimer.h"


#include "ThostFtdcMdApi.h"

class CMdSpi : public CThostFtdcMdSpi
{
public:
    CMdSpi(CThostFtdcMdApi *pUserApi)
    {
        m_iLoginMD = 0;
        m_pUserApi = pUserApi;
    }
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
    int m_iLoginMD;

private:
    // void ReqUserLogin();
    bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);
    uint64_t tempTimeStamp;
    CThostFtdcMdApi *m_pUserApi;
};

#endif

#include <iostream>
#include "string.h"

#include "marketdata/MdSpi.h"
#include "marketdata/MDHandler.h"
#include "infra/LogFacade.h"
#include "infra/Metrics.h"
#include "session/ORSServer.h"
#include "infra/Time.h"

using namespace std;

#include "ThostFtdcTraderApi.h"
// extern CThostFtdcMdApi* g_pUserApi;

extern char ppInstrumentID[200][30];
extern int iInstrumentID;

struct tm tepoch = {0};

bool CMdSpi::waitForLogin(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(loginMutex_);
    return loginCv_.wait_for(lock, timeout,
                             [this] { return m_iLoginMD.load() != 0; }) &&
           m_iLoginMD.load() != 0;
}

void CMdSpi::markLogin(int value) {
    {
        std::lock_guard<std::mutex> lock(loginMutex_);
        m_iLoginMD.store(value);
    }
    loginCv_.notify_all();
}

void CMdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << endl;
    IsErrorRspInfo(pRspInfo);
}

void CMdSpi::OnFrontDisconnected(int nReason)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "Reason = " << nReason << endl;
}

void CMdSpi::OnHeartBeatWarning(int nTimeLapse)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "nTimeLapse = " << nTimeLapse << endl;
}

void CMdSpi::OnFrontConnected()
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << endl;
    ReqUserLogin((char *)server_.m_BROKER_ID.c_str(), (char *)server_.m_INVESTOR_ID.c_str(), (char *)server_.m_PASSWORD.c_str());
}

void CMdSpi::ReqUserLogin(TThostFtdcBrokerIDType BROKER_ID, TThostFtdcInvestorIDType INVESTOR_ID, TThostFtdcPasswordType PASSWORD)
{
    CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.BrokerID, BROKER_ID);
    strcpy(req.UserID, BROKER_ID);
    strcpy(req.Password, PASSWORD);
    int iResult = m_pUserApi->ReqUserLogin(&req, ++requestId_);
    {
        std::lock_guard<std::mutex> lock(loginMutex_);
        m_iLoginMD.store(!iResult);
    }
    loginCv_.notify_all();
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "ReqUserLogin CMdSpi " << ((iResult == 0) ? "done " : "not done ") << endl;
    if (iResult)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "iResult " << iResult << std::endl;
        return;
    }
}

void CMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    const char *szLocalDate;
    long lDate;
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "|" << __FUNCTION__ << endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "OnRspUserLogin" << endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "ErrorCode=" << pRspInfo->ErrorID << " ErrorMsg=" << pRspInfo->ErrorMsg << endl;
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "RequestID=" << nRequestID << " Chain=" << bIsLast << endl;
    if (bIsLast && !IsErrorRspInfo(pRspInfo))
    {
        // szLocalDate = m_pUserApi->GetTradingDay();
        std::string cur_day = std::string{};

        lDate = atol(cur_day.c_str());
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Trading day " << cur_day.c_str() << endl;
        tepoch = {0};
        tepoch.tm_year = (lDate / 10000) - 1900;
        tepoch.tm_mon = (lDate / 100) % 100 - 1;
        tepoch.tm_mday = (lDate) % 100;
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Year " << tepoch.tm_year + 1900 << " Mon " << tepoch.tm_mon << " Day " << tepoch.tm_mday << std::endl;
        // SubscribeMarketData();
    }
}

void CMdSpi::SubscribeMarketData()
{
    int iResult;
    iResult = -1;

    // iResult = m_pUserApi->SubscribeMarketData((char**)&ppInstrumentID[0], iInstrumentID);
    for (SymbolMapIter iter = server_.m_symbolMap.begin(); iter != server_.m_symbolMap.end(); iter++)
    {
        char *temp = (char *)iter->first.c_str();
        iResult = m_pUserApi->SubscribeMarketData(&temp, 1);
    }
    ::ors::log::info("ors.md.subscribe",
              {{"result", iResult},
               {"message", iResult == 0 ? "market data subscription requested"
                                         : "market data subscription failed"}});
}

void CMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pSpecificInstrument == NULL)
    {
        ::ors::log::warn("ors.md.subscribe_response",
                  {{"message", "market data subscription response missing instrument"}});
        return;
    }
    ::ors::log::info("ors.md.subscribe_response",
              {{"instrument", pSpecificInstrument->InstrumentID},
               {"message", "market data subscription response"}});
    IsErrorRspInfo(pRspInfo);
}

void CMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << endl;
}

time_t parseTime(char szTimeInfo[], int iMillisecs)
{
    // day has been initialized at login
    sscanf(szTimeInfo, "%d:%d:%d", &tepoch.tm_hour, &tepoch.tm_min, &tepoch.tm_sec);
    time_t timeSinceEpoch = mktime(&tepoch);
    timeSinceEpoch = timeSinceEpoch * 1000 + iMillisecs;
    return timeSinceEpoch;
}

void CMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
    // std::cout << __FUNCTION__ << endl;
    ::ors::log::info("ors.md.tick",
              {{"instrument", pDepthMarketData->InstrumentID},
               {"update_time", pDepthMarketData->UpdateTime},
               {"update_millisec", pDepthMarketData->UpdateMillisec},
               {"message", "market data tick"}});
    strcpy(marketUpdate_.m_symbol, pDepthMarketData->InstrumentID);
    marketUpdate_.m_lastTradedPrice = pDepthMarketData->LastPrice;
    marketUpdate_.m_totalTradedQuantity = pDepthMarketData->Volume;
    marketUpdate_.m_exchTS = parseTime(pDepthMarketData->UpdateTime, pDepthMarketData->UpdateMillisec);
    marketUpdate_.m_lastTradedTime = marketUpdate_.m_exchTS;
    marketUpdate_.m_lastTradedQuantity = 0;
    marketUpdate_.m_totalTradedValue = pDepthMarketData->Turnover;
    marketUpdate_.m_validBids = 0;
    marketUpdate_.m_validAsks = 0;
    if (pDepthMarketData->BidVolume1 != 0)
    {
        marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].price = pDepthMarketData->BidPrice1;
        marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].quantity = pDepthMarketData->BidVolume1;
        marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].orderCount = 1;
        marketUpdate_.m_validBids++;
        if (pDepthMarketData->BidVolume2 != 0)
        {
            marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].price = pDepthMarketData->BidPrice2;
            marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].quantity = pDepthMarketData->BidVolume2;
            marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].orderCount = 1;
            marketUpdate_.m_validBids++;

            if (pDepthMarketData->BidVolume3 != 0)
            {
                marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].price = pDepthMarketData->BidPrice3;
                marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].quantity = pDepthMarketData->BidVolume3;
                marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].orderCount = 1;
                marketUpdate_.m_validBids++;

                if (pDepthMarketData->BidVolume4 != 0)
                {
                    marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].price = pDepthMarketData->BidPrice4;
                    marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].quantity = pDepthMarketData->BidVolume4;
                    marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].orderCount = 1;
                    marketUpdate_.m_validBids++;

                    if (pDepthMarketData->BidVolume5 != 0)
                    {
                        marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].price = pDepthMarketData->BidPrice5;
                        marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].quantity = pDepthMarketData->BidVolume5;
                        marketUpdate_.m_bidUpdates[marketUpdate_.m_validBids].orderCount = 1;
                        marketUpdate_.m_validBids++;
                    }
                }
            }
        }
    }

    if (pDepthMarketData->AskVolume1 != 0)
    {
        marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].price = pDepthMarketData->AskPrice1;
        marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].quantity = pDepthMarketData->AskVolume1;
        marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].orderCount = 1;
        marketUpdate_.m_validAsks++;
        if (pDepthMarketData->AskVolume2 != 0)
        {
            marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].price = pDepthMarketData->AskPrice2;
            marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].quantity = pDepthMarketData->AskVolume2;
            marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].orderCount = 1;
            marketUpdate_.m_validAsks++;
            if (pDepthMarketData->AskVolume3 != 0)
            {
                marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].price = pDepthMarketData->AskPrice3;
                marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].quantity = pDepthMarketData->AskVolume3;
                marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].orderCount = 1;
                marketUpdate_.m_validAsks++;
                if (pDepthMarketData->AskVolume4 != 0)
                {
                    marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].price = pDepthMarketData->AskPrice4;
                    marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].quantity = pDepthMarketData->AskVolume4;
                    marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].orderCount = 1;
                    marketUpdate_.m_validAsks++;
                    if (pDepthMarketData->AskVolume5 != 0)
                    {
                        marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].price = pDepthMarketData->AskPrice5;
                        marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].quantity = pDepthMarketData->AskVolume5;
                        marketUpdate_.m_askUpdates[marketUpdate_.m_validAsks].orderCount = 1;
                        marketUpdate_.m_validAsks++;
                    }
                }
            }
        }
    }
    // marketUpdate_.printLine();
    mdHandler_.ProcessMDUpdate(reinterpret_cast<char*>(&marketUpdate_));
    server_.getRms().updateLTPFromMD(&marketUpdate_);
}

bool CMdSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
    bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
    if (bResult)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "|ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
    }
    return bResult;
}

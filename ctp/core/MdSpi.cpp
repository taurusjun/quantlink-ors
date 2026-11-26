#include <iostream>
#include "string.h"

#include "MdSpi.h"
#include "MDHandler.h"
#include "log/Log.h"
#include "log/Metrics.h"
#include "ORSServer.h"
#include "core/Time.h"
#include "processsettings.h"

using namespace std;

#include "ThostFtdcTraderApi.h"
// extern CThostFtdcMdApi* g_pUserApi;

extern char ppInstrumentID[200][30];
extern int iInstrumentID;

extern int g_RequestId;
extern struct MarketUpdateNew *g_pUpdate;
extern MDHandler *g_pMDHandler;
extern ORSServer *g_pORS;

struct tm tepoch = {0};
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
    ReqUserLogin((char *)g_pORS->m_BROKER_ID.c_str(), (char *)g_pORS->m_INVESTOR_ID.c_str(), (char *)g_pORS->m_PASSWORD.c_str());
}

void CMdSpi::ReqUserLogin(TThostFtdcBrokerIDType BROKER_ID, TThostFtdcInvestorIDType INVESTOR_ID, TThostFtdcPasswordType PASSWORD)
{
    CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.BrokerID, BROKER_ID);
    strcpy(req.UserID, BROKER_ID);
    strcpy(req.Password, PASSWORD);
    int iResult = m_pUserApi->ReqUserLogin(&req, ++g_RequestId);
    m_iLoginMD = !iResult;
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "ReqUserLogin CMdSpi " << ((iResult == 0) ? "done " : "not done ") << endl;
    if (iResult)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "iResult " << iResult << std::endl;
        exit(1);
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
    for (SymbolMapIter iter = g_pORS->m_symbolMap.begin(); iter != g_pORS->m_symbolMap.end(); iter++)
    {
        char *temp = (char *)iter->first.c_str();
        iResult = m_pUserApi->SubscribeMarketData(&temp, 1);
    }
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "SubscribeMarketData " << ((iResult == 0) ? "done " : "not done ") << endl;
}

void CMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    tempTimeStamp = ::ors::nowNanos();
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|" << __FUNCTION__ << endl;
    if (pSpecificInstrument == NULL)
    {
        tempTimeStamp = ::ors::nowNanos();
        ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
                   << "Error in subscription " << std::endl;
        return;
    }
    ORS_LOG(INFO) << "|" << tempTimeStamp << "|"
               << "Instrument " << pSpecificInstrument->InstrumentID << std::endl;
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
    ORS_LOG(INFO) << "| RECVMD: " << pDepthMarketData->InstrumentID << "|" << pDepthMarketData->UpdateTime << endl;
    strcpy(g_pUpdate->m_symbol, pDepthMarketData->InstrumentID);
    g_pUpdate->m_lastTradedPrice = pDepthMarketData->LastPrice;
    g_pUpdate->m_totalTradedQuantity = pDepthMarketData->Volume;
    g_pUpdate->m_exchTS = parseTime(pDepthMarketData->UpdateTime, pDepthMarketData->UpdateMillisec);
    g_pUpdate->m_lastTradedTime = g_pUpdate->m_exchTS;
    g_pUpdate->m_lastTradedQuantity = 0;
    g_pUpdate->m_totalTradedValue = pDepthMarketData->Turnover;
    g_pUpdate->m_validBids = 0;
    g_pUpdate->m_validAsks = 0;
    if (pDepthMarketData->BidVolume1 != 0)
    {
        g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].price = pDepthMarketData->BidPrice1;
        g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].quantity = pDepthMarketData->BidVolume1;
        g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].orderCount = 1;
        g_pUpdate->m_validBids++;
        if (pDepthMarketData->BidVolume2 != 0)
        {
            g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].price = pDepthMarketData->BidPrice2;
            g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].quantity = pDepthMarketData->BidVolume2;
            g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].orderCount = 1;
            g_pUpdate->m_validBids++;

            if (pDepthMarketData->BidVolume3 != 0)
            {
                g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].price = pDepthMarketData->BidPrice3;
                g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].quantity = pDepthMarketData->BidVolume3;
                g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].orderCount = 1;
                g_pUpdate->m_validBids++;

                if (pDepthMarketData->BidVolume4 != 0)
                {
                    g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].price = pDepthMarketData->BidPrice4;
                    g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].quantity = pDepthMarketData->BidVolume4;
                    g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].orderCount = 1;
                    g_pUpdate->m_validBids++;

                    if (pDepthMarketData->BidVolume5 != 0)
                    {
                        g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].price = pDepthMarketData->BidPrice5;
                        g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].quantity = pDepthMarketData->BidVolume5;
                        g_pUpdate->m_bidUpdates[g_pUpdate->m_validBids].orderCount = 1;
                        g_pUpdate->m_validBids++;
                    }
                }
            }
        }
    }

    if (pDepthMarketData->AskVolume1 != 0)
    {
        g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].price = pDepthMarketData->AskPrice1;
        g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].quantity = pDepthMarketData->AskVolume1;
        g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].orderCount = 1;
        g_pUpdate->m_validAsks++;
        if (pDepthMarketData->AskVolume2 != 0)
        {
            g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].price = pDepthMarketData->AskPrice2;
            g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].quantity = pDepthMarketData->AskVolume2;
            g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].orderCount = 1;
            g_pUpdate->m_validAsks++;
            if (pDepthMarketData->AskVolume3 != 0)
            {
                g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].price = pDepthMarketData->AskPrice3;
                g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].quantity = pDepthMarketData->AskVolume3;
                g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].orderCount = 1;
                g_pUpdate->m_validAsks++;
                if (pDepthMarketData->AskVolume4 != 0)
                {
                    g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].price = pDepthMarketData->AskPrice4;
                    g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].quantity = pDepthMarketData->AskVolume4;
                    g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].orderCount = 1;
                    g_pUpdate->m_validAsks++;
                    if (pDepthMarketData->AskVolume5 != 0)
                    {
                        g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].price = pDepthMarketData->AskPrice5;
                        g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].quantity = pDepthMarketData->AskVolume5;
                        g_pUpdate->m_askUpdates[g_pUpdate->m_validAsks].orderCount = 1;
                        g_pUpdate->m_validAsks++;
                    }
                }
            }
        }
    }
    // g_pUpdate->printLine();
    g_pMDHandler->ProcessMDUpdate((char *)g_pUpdate);
    g_pORS->getRms().updateLTPFromMD(g_pUpdate);
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

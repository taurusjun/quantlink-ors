/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */

#ifndef RMSTESTBASE_H
#define RMSTESTBASE_H

#include "rmsimpl.h"
#include <string>
#include <map>

const int RMSTEST_MAXSTRATEGY = 10;

struct RMSTestInstDet
{
    double RoundLot;
    double Factor;
};

class RMSTestBase
{

public:
    RMSTestBase(std::string rmsConfig);
    ~RMSTestBase();
    void printReport();

protected:
    virtual void instrumentInit() = 0;
    void beginTest();
    void endTest();
    bool newOrder(int strategyId, std::string account, std::string symbol, double price, int32_t qty, unsigned char side);
    bool modifyOrder(int strategyId, int newOrderOffset, std::string account, std::string symbol, double price, int32_t qty, unsigned char side);
    bool cancelOrder(int strategyId, int newOrderOffset, std::string account, std::string symbol, double price, int32_t qty, unsigned char side);
    bool newConfirm(int strategyId, int neworderOffset);
    bool modifyConfirm(int strategyId, int neworderOffset);
    bool cancelConfirm(int strategyId, int neworderOffset);
    bool tradeConfirm(int strategyId, int neworderOffset, std::string symbol, double price, int32_t qty, unsigned char side);
    bool newReject(int strategyId, int neworderOffset);
    bool modifyReject(int strategyId, int neworderOffset);
    bool cancelReject(int strategyId, int neworderOffset);
    void printResponse(illuminati::infra::ResponseType responseType, uint32_t orderId, uint32_t errorCode);
    illuminati::infra::RequestMsg getRequest(std::string account, std::string symbol, double price, int32_t qty, unsigned char side, uint32_t orderId, illuminati::infra::RequestType reqType);
    bool ADDREPORT(std::string str, bool value);

    template <typename T>
    void setExchangeInstrDefinitionMap(std::unordered_map<std::string, T> &xIntrDef,
                                       bool useRoundLotForValueCalc,
                                       bool usePriceFactorForValueCalc,
                                       bool applyRoundLotValueOnIncomingQuanityChecks)
    {
        m_rms->setExchangeInstrDefinitionMap<T>(xIntrDef, useRoundLotForValueCalc, usePriceFactorForValueCalc, applyRoundLotValueOnIncomingQuanityChecks);
    }

private:
    void initOrderId();
    std::string m_rmsConfig;
    std::string m_currentTest;
    std::string m_lastError;
    illuminati::infra::RmsImpl *m_rms;
    uint32_t m_orderId[RMSTEST_MAXSTRATEGY];

    struct report_result
    {
        int successCount;
        int failureCount;
        std::string lastErrorMessage;
    };
    std::map<std::string, report_result> m_report;
};

#endif // RMSTESTBASE_H

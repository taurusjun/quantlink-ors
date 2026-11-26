#ifndef __RMS_IMPL_HPP
#define __RMS_IMPL_HPP

#include <string>
#include <cassert>
#include "rmsimpl.h"
#include "logger.h"
#include "constants.h"
#include "orderresponse.h"
#include "rmsloginstruct.h"

using namespace std;
using namespace illuminati::md;
using namespace illuminati::stats;
using namespace illuminati::infra;

/*!
 * ctor
 */
RmsImpl::RmsImpl()
{
    m_orderFrequency = NULL;
    m_orderFrequencyEnabled = false;
    m_orderFrequencyWindow = 1;
    m_rmsConfigFile = "";
    m_cancelConfirmsCountCheckEnabled = false;
}
/*!
 * ~RmsImpl
 */
RmsImpl::~RmsImpl()
{
    cleanupRmsImpl();
}
/*!
 * initRmsImpl
 */

void RmsImpl::initRmsImpl(int maxShmClients, string rmsConfig,
                          std::string rmsFirmwideConfig, std::string rmsFirmiwdeConfigKey)
{
    if (maxShmClients < 1 || maxShmClients > MAX_ORS_CLIENTS)
    {
        throw string(
            "MAX SHM CLIENTS is NOT correctly set in config. Used for reserving total positions array size");
    }
#ifdef _RMS_STATS
    NEW_ORDER_ONE = StatsRecorder::registerNewStat("NEW_ORDER", 1000, 5000);
    MODIFY_ORDER_ONE = StatsRecorder::registerNewStat("MODIFY_ORDER", 1000, 5000);
    CANCEL_ORDER_ONE = StatsRecorder::registerNewStat("CANCEL_ORDER", 1000, 5000);
#endif

    m_rmsConfigFile = rmsConfig;
    m_rmsFirmwideConfigFile = rmsFirmwideConfig;
    m_rmsFirmwideConfigKey = rmsFirmiwdeConfigKey;

    reset();
    m_orderPositionsPool.allocate(100000);
}
/*!
 * reset
 * call this to reload the configuration
 */
void RmsImpl::reset()
{
    m_criticalSection.lock();
    m_rmsChecks.lock();

    illuminati::RmsLoginStruct login;
    // reseting the key
    m_rmsConfig.setFirmwideConfigKey("");
    try
    {
        login.load(m_rmsFirmwideConfigKey);
        ILOG(INFO) << "RMS encryption key = " << login.key << "\n";
        m_rmsConfig.setFirmwideConfigKey(login.key);
    }
    catch (const char *strex)
    {
        ILOG(FATAL) << "Caught exception while loading rms key file, ignoring the key. " << strex << "\n";
    }
    catch (std::string &strex)
    {
        ILOG(FATAL) << "Caught exception while loading rms key file, ignoring the key. " << strex << "\n";
    }
    m_rmsConfig.loadConfig(m_rmsConfigFile, m_rmsFirmwideConfigFile);
    m_rmsConfig.print();

    // Set the ltp not set map
    auto symbolSet = m_rmsConfig.getEnabledSymbols();
    for (auto i = symbolSet.begin(); i != symbolSet.end(); ++i)
    {
        m_ticketIsLTPSet.insert(make_pair((*i), false));
    }

    loadAccountPosCacheAndRMSLimits();
    m_rmsChecks.reset(m_accountPosStartUpCache, m_instrDefCache, m_rmsConfig);
    m_orderFrequencyEnabled = m_rmsChecks.OrderFrequencyCheckEnabled();
    m_orderFrequencyWindow = m_rmsChecks.GetOrderFrequencyWindow();
    m_orderPerSecondLimit = m_rmsChecks.GetOrderPerSecondLimit();
    m_orderFreqCalcMode = m_rmsChecks.GetOrderPerSecondCalcMode();

    /// firm account level container which checks for total orders sent per second.
    if (m_orderFrequencyEnabled)
    {
        m_orderFrequency = new OrderFrequency(m_orderPerSecondLimit);
    }
    else
    {
        m_orderFrequency = new OrderFrequency(1);
    }

    m_cancelConfirmsCountCheckEnabled =
        m_rmsChecks.IsCancelConfirmsCountCheckEnabled();

    m_rmsChecks.unlock();
    m_criticalSection.unlock();
}
/*!
 * load rms threshold, they reside in
 */
void RmsImpl::loadAccountPosCacheAndRMSLimits()
{
    std::vector<std::string> accounts = m_rmsConfig.getAllAccounts();
    for (int i = 0; i < accounts.size(); i++)
    {
        std::string accountid;
        setAccountValuesInCorrectFormat(accounts[i], accountid,
                                        MAX_ACCNTID_LEN);
        AccountPositions *pAcc = NULL;
        auto accCacheIt = m_accountPosStartUpCache.find(accountid);
        if (m_accountPosStartUpCache.end() == accCacheIt)
        {
            pAcc = new AccountPositions(accountid);
            m_accountPosStartUpCache.insert(make_pair(accountid, pAcc));
            std::cout << accountid << std::endl;
        }
        else
        {
            pAcc = accCacheIt->second;
        }

        pAcc->m_accountLimits = m_rmsConfig.getAccountLimits(accounts[i]);
        if (!pAcc->m_accountLimits)
        {
            ILOG(FATAL) << "No limits configured for account: " << accounts[i] << "\n";
            throw "No limits configured for account";
        }
        ILOG(INFO) << "Setting account limits to " << accounts[i] << "\n";
        pAcc->m_accountLimits->printLimits();

        for (auto index = pAcc->perTickerPositions.begin();
             index != pAcc->perTickerPositions.end(); index++)
        {
            TickerPositions *tickPos = index->second;
            tickPos->thresholds = pAcc->m_accountLimits->getInstrumentLimits(
                tickPos->symbol);
        }
    }
}
/*!
 * getAccountPosFromCache
 * returns NULL is account is not configured
 */
AccountPositions *RmsImpl::getAccountPosFromCache(string &accountId)
{
    AccountPositions *pAcc = NULL;
    auto accCacheIt = m_accountPosStartUpCache.find(accountId);
    if (accCacheIt != m_accountPosStartUpCache.end())
    {
        pAcc = accCacheIt->second;
    }

    return pAcc;
}
/*!
 * getTickerPosFromCache
 * returns NULL is ticker is not configured
 */
TickerPositions *RmsImpl::getTickerPosFromCache(const string &ticker)
{
    TickerPositions *tick = NULL;
    auto tickCacheIt = m_instrDefCache.find(ticker);
    if (tickCacheIt != m_instrDefCache.end())
    {
        tick = tickCacheIt->second;
    }

    return tick;
}
/*!
 * updateLTPFromMD
 * This will be usually called from a seperate thread.
 */
void RmsImpl::updateLTPFromMD(const illuminati::md::MarketUpdateNew *mdUpdate)
{
    if (mdUpdate == NULL || mdUpdate->m_endPkt)
        return;

    double ltpAlternative = 0.0;
    double lastTradePrice = mdUpdate->m_lastTradedPrice;

    bool isTrade = (MDUPDTYPE_TRADE_IMPLIED == mdUpdate->m_updateType) || (MDUPDTYPE_TRADE == mdUpdate->m_updateType) || (MDUPDTYPE_TRADE_INFO == mdUpdate->m_updateType);

    auto iter = m_ticketIsLTPSet.find(mdUpdate->m_symbol);
    bool isLTP0 = (iter != m_ticketIsLTPSet.end()) && (iter->second == false);

    if (isLTP0)
    {
        if (mdUpdate->m_validAsks == 0 && mdUpdate->m_validBids == 0)
            ltpAlternative = 0;
        else if (mdUpdate->m_validAsks == 0)
            ltpAlternative = mdUpdate->m_bidUpdates[0].price;
        else if (mdUpdate->m_validBids == 0)
            ltpAlternative = mdUpdate->m_askUpdates[0].price;
        else
            ltpAlternative = (mdUpdate->m_bidUpdates[0].price + mdUpdate->m_bidUpdates[0].price) / 2;

        iter->second = true;
    }

    if (isTrade || isLTP0)
    {
        string symbol = mdUpdate->m_symbol;
        m_syncLTPUpdates.lock();
        auto iter = m_tickerToLTUpdates.find(symbol);
        if (iter == m_tickerToLTUpdates.end())
        {
            TickerLTPUpdates *ltpUpdate = new TickerLTPUpdates(lastTradePrice,
                                                               ltpAlternative);
            auto ret = m_tickerToLTUpdates.insert(
                make_pair(symbol, ltpUpdate));
        }
        else
        {
            TickerLTPUpdates &ltpUpdate = *(iter->second);
            ltpUpdate.lock();
            ltpUpdate.lastTradePrice = lastTradePrice;
            ltpUpdate.ltpAlternative = ltpAlternative;
            ltpUpdate.unlock();
        }
        m_syncLTPUpdates.unlock();
    }
}
/*!
 * executeNewOrderChecks
 */
template <typename SYMBOL_NAMING_POLICY>
bool RmsImpl::executeNewOrderChecks(const illuminati::infra::RequestMsg &req)
{
    int32_t rmsError = E_RMS_NO_ERROR;
    OrderPositions *totalPos = getTradingStatSoFar<SYMBOL_NAMING_POLICY>(req);
    if (NULL == totalPos)
    {
        ILOG(FATAL) << "RmsErrorCode: " << m_rmsStateError << " Desc:" << getErrorCodeDesciption(m_rmsStateError) << " RequestDetails:" << req.toStringLine() << "\n";
        EMIT_PARAM(RMS_ERROR, ResponseType::RMS_REJECT, req.OrderID, m_rmsStateError);
        return false;
    }

    bool bRmsCheckSuccess = m_rmsChecks.execute(req, *totalPos, rmsError);
    if (!bRmsCheckSuccess)
    {
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        if (rmsError == E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_BREACHED || rmsError == E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_BREACHED)
        {
            EMIT_PARAM(RMS_ERROR, ResponseType::ORDERS_PER_DAY_LIMIT_REJECT, req.OrderID, rmsError);
        }
        else
        {
            EMIT_PARAM(RMS_ERROR, ResponseType::RMS_REJECT, req.OrderID, rmsError);
        }
        printAllPositions();
    }
    else
    {
        if (rmsError == E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_WARNING || rmsError == E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::ORDERS_PER_DAY_LIMIT_WARNING, req.OrderID, rmsError);
        }
        else if (rmsError == E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::STOP_LOSS_WARNING, req.OrderID, rmsError);
        }
        else if (rmsError == E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::STOP_LOSS_WARNING, req.OrderID, rmsError);
        }

        incrementTradingStatsOnNewOrder(*totalPos, req);
        storeOrderStats(*totalPos, req);
    }
    return bRmsCheckSuccess;
}
/*!
 * executeModifyOrderChecks
 */
template <typename SYMBOL_NAMING_POLICY>
bool RmsImpl::executeModifyOrderChecks(const RequestMsg &req)
{
    int32_t rmsError = E_RMS_NO_ERROR;
    m_criticalSection.lock();
    OrderPositions *totalPos = getOrderPositions(req.OrderID);

    if (NULL == totalPos)
    {
        rmsError = E_RMS_UNEXPECTED_ORDERID;
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        EMIT_PARAM(RMS_ERROR,
                   ResponseType::MODIFY_ORDER_REJECT, req.OrderID, rmsError);
        m_criticalSection.unlock();
        return false;
    }

    if (totalPos->doneQty != req.QuantityFilled)
    {
        rmsError = E_RMS_ORDER_LEVEL_FILL_QUANTITY_MISMATCH;
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        ILOG(FATAL) << "Done quantity was " << totalPos->doneQty << " while QuantityFilled sent in the request message was " << req.QuantityFilled << "\n";
        EMIT_PARAM(RMS_ERROR, ResponseType::MODIFY_ORDER_REJECT, req.OrderID, rmsError);
        m_criticalSection.unlock();
        return false;
    }

    // only changed on the request thread so no lock acquired.
    totalPos->orderTime = req.TimeStamp;
    bool bRmsCheckSuccess = m_rmsChecks.execute(req, *totalPos, rmsError);
    if (!bRmsCheckSuccess)
    {
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        if (rmsError == E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_BREACHED || rmsError == E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_BREACHED)
        {
            EMIT_PARAM(RMS_ERROR, ResponseType::ORDERS_PER_DAY_LIMIT_REJECT, req.OrderID, rmsError);
        }
        else
        {
            EMIT_PARAM(RMS_ERROR, ResponseType::RMS_REJECT, req.OrderID, rmsError);
        }
        printAllPositions();
    }
    else
    {
        if (rmsError == E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_WARNING || rmsError == E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::ORDERS_PER_DAY_LIMIT_WARNING, req.OrderID, rmsError);
        }
        else if (rmsError == E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::STOP_LOSS_WARNING, req.OrderID, rmsError);
        }
        else if (rmsError == E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::STOP_LOSS_WARNING, req.OrderID, rmsError);
        }

        incrementOrDecrementTradingStatsOnModifyOrder(*totalPos, req);
    }

    m_criticalSection.unlock();

#if defined(_PRINT_UPDATE)
    printOrderStats(req.OrderID);
#endif

    return bRmsCheckSuccess;
}
/*!
 * executeCancelOrderChecks
 */
template <typename SYMBOL_NAMING_POLICY>
bool RmsImpl::executeCancelOrderChecks(
    const illuminati::infra::RequestMsg &req)
{
    int32_t rmsError;
    ResponseType responseType;
    bool bRmsCheckSuccess = checkAndUpdateTradingStatsOnCancel(req, rmsError,
                                                               responseType);
    if (!bRmsCheckSuccess)
    {
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        EMIT_PARAM(RMS_ERROR, responseType, req.OrderID, rmsError);
        printAllPositions();
    }
    return bRmsCheckSuccess;
}

/*!
 * getTradingStatSoFar -- get called from the Request thread
 * Will return NULL if the req.AccountID is not configured in the RMS config file
 */
template <typename SYMBOL_NAMING_POLICY>
OrderPositions *RmsImpl::getTradingStatSoFar(const RequestMsg &req)
{
    // if this is mod, cancel or a query return from the available pos
    // with the ordertime updated to current request
    if (RequestType::NEWORDER != req.Request_Type)
    {
        OrderPositions *totalPos = NULL;
        m_criticalSection.lock();
        totalPos = getOrderPositions(req.OrderID);
        if (totalPos != NULL)
        {
            totalPos->orderTime = req.TimeStamp;
        }
        m_criticalSection.unlock();
        return totalPos;
    }

    // This is a new order, create a OrderPostion Object and getother stuffs
    string accountId = req.AccountID;
    string ticker = SYMBOL_NAMING_POLICY::getSymbolName(req); // or expose a getSymbolMethod
    OrderPositions *totalPos = m_orderPositionsPool.getObject(req.OrderID);
    // OrderPositions * totalPos = new OrderPositions(req.OrderID);

    totalPos->side = req.Transaction_Type;
    totalPos->orderTime = req.TimeStamp;
    totalPos->allAccountsPointer = &m_accountPosStartUpCache;

    if (NULL == m_orderFrequency)
    {
        m_rmsStateError = E_RMS_INCOMPLETE_INIT;
        ILOG(FATAL) << "Order Per Second queue is not constructed. Should have been done at the init phase \n";
        return NULL;
    }
    totalPos->ordersPerSecond = m_orderFrequency;

    auto accniter = m_accountPosStartUpCache.find(accountId);
    if (accniter == m_accountPosStartUpCache.end())
    {
        m_rmsStateError = E_RMS_BLOCKED_ACCOUNT;
        ILOG(FATAL) << "account " << accountId << " is not configured in the RMS config \n";
        return NULL;
    }
    AccountPositions &accountPositions = *(accniter->second);
    totalPos->accountIterator = accniter;

    // Inserting into map so need to lock here
    accountPositions.m_tickerPositionslock.lock();
    auto tickiter = accountPositions.perTickerPositions.find(ticker);
    if (tickiter == accountPositions.perTickerPositions.end())
    {
        TickerPositions *tickFromCache = getTickerPosFromCache(ticker);
        if (NULL == tickFromCache)
        {
            accountPositions.m_tickerPositionslock.unlock();
            m_rmsStateError = E_RMS_BLOCKED_SYMBOL;
            ILOG(FATAL) << "symbol " << ticker << " is not found in instrument cache \n";
            return NULL;
        }
        TickerPositions *newTicker = new TickerPositions(tickFromCache);
        newTicker->thresholds =
            accountPositions.m_accountLimits->getInstrumentLimits(ticker);

#if defined(_PRINT_UPDATE)
        ILOG(INFO) << "New Ticker (" << ticker << ") Thresholds - \n";
        newTicker->thresholds->printLimits();
#endif

        auto ret = accountPositions.perTickerPositions.insert(
            pair<string, TickerPositions *>(ticker, newTicker));
        tickiter = ret.first; /*! assuming insert always succeeds thats why no check for ret.second == true */
    }
    accountPositions.m_tickerPositionslock.unlock();

    totalPos->tickerIterator = tickiter;

    m_syncLTPUpdates.lock();
    auto ltpIter = m_tickerToLTUpdates.find(ticker);
    if (ltpIter == m_tickerToLTUpdates.end())
    {
        TickerLTPUpdates *tickUpdate = new TickerLTPUpdates();
        auto ret = m_tickerToLTUpdates.insert(make_pair(ticker, tickUpdate));
        ltpIter = ret.first;
    }
    totalPos->tickerIterator->second->tickLtpIterator = ltpIter;
    m_syncLTPUpdates.unlock();

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "Ticker Thresholds \n";
    totalPos->tickerIterator->second->thresholds->printLimits();
#endif

    return totalPos;
}
/*!
 * decrementTradingStatsOnCxlOrderConfirm
 */
void RmsImpl::handleCxlOrderConfirm(const ResponseMsg &resp)
{
    bool errFlag = false;
    double orderQty = 0.0;
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter == m_orderToPositions.end())
    {
        errFlag = true;
    }
    else
    {
        OrderPositions &pos = *(iter->second);
        pos.lock();
        /*! for IOC, FOC, FAK orders cancel confirmation can come without order confirmation */
        if (!pos.confirmed)
            pos.confirm();
        /*! response quantity can be greater than the current open qty on the order */
        orderQty = pos.openQty;  /*! taking a copy of the order quantity */
        pos.openQty -= orderQty; /*! reducing the open quantity on the order by it's own quantity; disregarding resp.Quantity */
        TickerPositions &tickerPos = *(pos.tickerIterator->second);
        tickerPos.lock();
        if (illuminati::md::SIDE_BUY == pos.side)
        {
            if (pos.openQty <= 0)
            {
                tickerPos.buyOpenOrdersCount--;
            }
            tickerPos.netBuyOpenQty -= orderQty;
            while (!pos.modifySentButNotConfirmedQtyQueue.empty())
            {
                tickerPos.netBuyModifySentButNotConfirmedQty -= pos.modifySentButNotConfirmedQtyQueue.front();
                pos.modifySentButNotConfirmedQty -= pos.modifySentButNotConfirmedQtyQueue.front();
                pos.modifySentButNotConfirmedQtyQueue.pop();
            }
        }
        else
        {
            if (pos.openQty <= 0)
            {
                tickerPos.sellOpenOrdersCount--;
            }
            tickerPos.netSellOpenQty -= orderQty;
            while (!pos.modifySentButNotConfirmedQtyQueue.empty())
            {
                tickerPos.netSellModifySentButNotConfirmedQty -= pos.modifySentButNotConfirmedQtyQueue.front();
                pos.modifySentButNotConfirmedQty -= pos.modifySentButNotConfirmedQtyQueue.front();
                pos.modifySentButNotConfirmedQtyQueue.pop();
            }
        }
        tickerPos.unlock();
        if (pos.openQty <= 0)
        {
            pos.unlock();
            m_orderToPositions.erase(iter);
        }
        else
        {
            pos.unlock();
        }
    }

    m_criticalSection.unlock();

    if (errFlag)
    {
        std::ostringstream css;
        css << "NO matching order in cache for this orderid " << resp.OrderID
            << " though we are in cxl confirm\n";
        std::string errMsg = css.str();
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {

#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
        printAllPositions();
#endif
    }
}
/*!
 * updateOrderFrequency
 */
void RmsImpl::updateOrderFrequency(OrderFrequency *orderFrequency,
                                   uint64_t requestorderTime)
{
    double orderTime = requestorderTime * 1.0;
    if (m_orderFreqCalcMode == ROLLING)
    {
        if (orderFrequency->m_orderQueue.isFull())
        {
            orderFrequency->m_orderQueue.dequeue();
        }
        orderFrequency->m_orderQueue.enqueue(orderTime);
    }
    else
    {
        if (orderFrequency->m_orderQueue.isEmpty())
        {
            orderFrequency->m_orderQueue.enqueue(orderTime);
            return;
        }

        double time_start = orderFrequency->getStartTime();
        double timeDiff = (orderTime - time_start);
        if (timeDiff >= m_orderFrequencyWindow)
        {
            orderFrequency->m_orderQueue.reset();
            orderFrequency->m_orderQueue.enqueue(orderTime);
        }
        else
        {
            orderFrequency->m_orderQueue.enqueue(orderTime);
        }
    }
}

/*!
 * incrementTradingStatsOnNewOrder
 */
void RmsImpl::incrementTradingStatsOnNewOrder(OrderPositions &orderPos,
                                              const RequestMsg &req)
{
    orderPos.lock(); /*! order has not been sent yet, no possibility of response thread updating orderPos */
    TickerPositions &tickPos = *(orderPos.tickerIterator->second);
    tickPos.lock(); /*! tickPos is a global object getting accessed from request and response threads */
    if (illuminati::md::SIDE_BUY == req.Transaction_Type)
    {
        tickPos.buyOpenOrdersCount++;
        tickPos.netBuyOpenQty += req.Quantity;
    }
    else
    {
        tickPos.sellOpenOrdersCount++;
        tickPos.netSellOpenQty += req.Quantity;
    }
    tickPos.unlock();

    // increment the total order per day stats for the account
    orderPos.accountIterator->second->ordersPerDay.incrementNewOrdersCount();

    orderPos.sentPrice = req.Price;
    orderPos.sentQty = req.Quantity;
    orderPos.orderID = req.OrderID;
    orderPos.lastRequestOutgoingRequest = RequestType::NEWORDER;
    orderPos.confirmed = false;
    orderPos.unlock();

    if (m_orderFrequencyEnabled)
    {
        updateOrderFrequency(orderPos.ordersPerSecond, orderPos.orderTime);
    }
#if defined(_PRINT_UPDATE)
    printOrderStats(req.OrderID);
#endif
}
/*!
 * incrementOrDecrementTradingStatsOnModifyOrder
 */
void RmsImpl::incrementOrDecrementTradingStatsOnModifyOrder(
    OrderPositions &orderPos, const RequestMsg &req)
{
    orderPos.lock(); /*! order has not been sent yet, no possibility of response thread updating orderPos */
    TickerPositions &tickPos = *(orderPos.tickerIterator->second);
    tickPos.lock(); /*! tickPos is a global object getting accessed from request and response threads */
    /// MODIFY is done to increase the quantity
    if (illuminati::md::SIDE_BUY == req.Transaction_Type)
    {
        // TODO we don't need to do calculate tickPos openQty now
        // tickPos.netBuyOpenQty += (req.Quantity - orderPos.openQty);
        tickPos.netBuyModifySentButNotConfirmedQty += req.Quantity;
        orderPos.modifySentButNotConfirmedQtyQueue.push(req.Quantity);
    }
    else if (illuminati::md::SIDE_SELL == req.Transaction_Type)
    {
        // TODO we don't need to do calculate tickPos openQty now
        // tickPos.netSellOpenQty += (req.Quantity - orderPos.openQty);
        tickPos.netSellModifySentButNotConfirmedQty += req.Quantity;
        orderPos.modifySentButNotConfirmedQtyQueue.push(req.Quantity);
    }
    orderPos.modifySentButNotConfirmedQty += req.Quantity;
    tickPos.unlock();

    orderPos.sentPrice = req.Price;
    orderPos.sentQty = req.Quantity;
    orderPos.sentQtyDelta = req.Quantity - orderPos.openQty;
    // increment the total order per day stats
    orderPos.accountIterator->second->ordersPerDay.incrementNewOrdersCount();
    orderPos.lastRequestOutgoingRequest = RequestType::MODIFYORDER;
    orderPos.unlock();

    if (m_orderFrequencyEnabled)
    {
        updateOrderFrequency(orderPos.ordersPerSecond, orderPos.orderTime);
    }
}
/*!
 * updateOrderStatsOnNewOrder
 */
void RmsImpl::handleNewOrderConfirm(const ResponseMsg &resp)
{
    bool errFlag = false;
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        orderPos.confirm();
        orderPos.unlock();
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();
    m_orderPositionsPool.addObjectToPool();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnNewOrder: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}
/*!
 * updateOrderStatsOnOrderModify
 */
void RmsImpl::handleModifyConfirm(const ResponseMsg &resp)
{
    bool errFlag = false;
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        /// update order's openQty with sentQtyDelta
        orderPos.openPrice = orderPos.sentPrice;

        // Just copy the open Qty directly instead of calculating it based
        // on delta
        // orderPos.openQty = orderPos.openQty + orderPos.sentQtyDelta;

        int64_t oldopenQty = orderPos.openQty;
        // Just copy the open quantity from response
        orderPos.openQty = resp.Quantity;

        TickerPositions &tickerPos = *(orderPos.tickerIterator->second);
        tickerPos.lock();

        if (illuminati::md::SIDE_BUY == orderPos.side)
        {
            if (orderPos.openQty <= 0)
            {
                tickerPos.buyOpenOrdersCount--;
            }

            if (!orderPos.modifySentButNotConfirmedQtyQueue.empty())
            {
                orderPos.modifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
                tickerPos.netBuyModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
                orderPos.modifySentButNotConfirmedQtyQueue.pop();
            }

            // Discard the old open quantity and replace it by new open quantity
            tickerPos.netBuyOpenQty += orderPos.openQty - oldopenQty;
        }
        else
        {
            if (orderPos.openQty <= 0)
            {
                tickerPos.sellOpenOrdersCount--;
            }

            if (!orderPos.modifySentButNotConfirmedQtyQueue.empty())
            {
                orderPos.modifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
                tickerPos.netSellModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
                orderPos.modifySentButNotConfirmedQtyQueue.pop();
            }

            // Discard the old open quantity and replace it by new open quantity
            tickerPos.netSellOpenQty += orderPos.openQty - oldopenQty;
        }
        tickerPos.unlock();

        orderPos.sentQty = 0;
        orderPos.sentPrice = 0;
        orderPos.sentQtyDelta = 0;
        if (orderPos.openQty <= 0)
        {
            orderPos.unlock();
            m_orderToPositions.erase(iter);
        }
        else
        {
            orderPos.unlock();
        }
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnNewOrder: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}
/*!
 * updateOrderStatsOnOrderReject
 */
void RmsImpl::handleOrderReject(const ResponseMsg &resp)
{
    bool errFlag = false;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        if (orderPos.confirmed)
        {
            orderPos.unconfirmNewOrder();
        }
        TickerPositions &tickPos = *(orderPos.tickerIterator->second);
        tickPos.lock();
        if (illuminati::md::SIDE_BUY == orderPos.side)
        {
            tickPos.buyOpenOrdersCount--;
            tickPos.netBuyOpenQty -= orderPos.sentQty;
        }
        else
        {
            tickPos.sellOpenOrdersCount--;
            tickPos.netSellOpenQty -= orderPos.sentQty;
        }
        tickPos.unlock();
        orderPos.unlock();
        m_orderToPositions.erase(iter);
        m_orderPositionsPool.addObjectToPool();
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnOrderReject: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {

#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}
/*!
 * updateOrderStatsOnOrderRejectFromMiddleSystem
 */
void RmsImpl::handleOrderRejectFromMiddleSystem(
    const ResponseMsg &resp)
{
    bool errFlag = false;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        if (orderPos.confirmed)
        {
            orderPos.unconfirmNewOrder();
        }
        TickerPositions &tickPos = *(orderPos.tickerIterator->second);
        tickPos.lock();
        if (illuminati::md::SIDE_BUY == orderPos.side)
        {
            tickPos.buyOpenOrdersCount--;
            tickPos.netBuyOpenQty -= orderPos.sentQty;
        }
        else
        {
            tickPos.sellOpenOrdersCount--;
            tickPos.netSellOpenQty -= orderPos.sentQty;
        }
        orderPos.accountIterator->second->ordersPerDay.decrementNewOrdersCount();
        tickPos.unlock();
        orderPos.unlock();
        m_orderToPositions.erase(iter);
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnOrderReject: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {

#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}
/*!
 * updateOrderStatsOnModifyOrderReject
 */
void RmsImpl::handleModifyOrderReject(const ResponseMsg &resp)
{
    bool errFlag = false;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        TickerPositions &tickPos = *(orderPos.tickerIterator->second);
        orderPos.lock();

        if (!orderPos.modifySentButNotConfirmedQtyQueue.empty())
        {
            // Get the sentQty for this modify confirm
            orderPos.modifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
            tickPos.lock();
            if (illuminati::md::SIDE_BUY == orderPos.side)
            {
                tickPos.netBuyModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
            }
            else
            {
                tickPos.netSellModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
            }
            tickPos.unlock();
            orderPos.modifySentButNotConfirmedQtyQueue.pop();
        }

        orderPos.sentQty = 0;
        orderPos.sentQtyDelta = 0;
        orderPos.sentPrice = 0;
        orderPos.unlock();
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnOrderReject: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {

#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}

void RmsImpl::handleModifyOrderRejectFromMiddleSystem(
    const ResponseMsg &resp)
{
    bool errFlag = false;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        TickerPositions &tickPos = *(orderPos.tickerIterator->second);
        orderPos.lock();

        if (!orderPos.modifySentButNotConfirmedQtyQueue.empty())
        {
            orderPos.modifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
            tickPos.lock();
            if (illuminati::md::SIDE_BUY == orderPos.side)
            {
                tickPos.netBuyModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
            }
            else
            {
                tickPos.netSellModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQtyQueue.front();
            }

            tickPos.unlock();
            orderPos.modifySentButNotConfirmedQtyQueue.pop();
        }
        orderPos.sentQty = 0;
        orderPos.sentPrice = 0;
        orderPos.sentQtyDelta = 0;
        orderPos.accountIterator->second->ordersPerDay.decrementNewOrdersCount();
        orderPos.unlock();
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnOrderReject: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {

#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}

void RmsImpl::handleCxlOrderRejectFromMiddleSystem(
    const ResponseMsg &resp)
{
    bool errFlag = false;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        orderPos.accountIterator->second->ordersPerDay.decrementCxlOrdersCount();
        TickerPositions &tickerPos = *(orderPos.tickerIterator->second);
        tickerPos.lock();
        tickerPos.totalCancelOrderConfirms--;
        tickerPos.unlock();
        orderPos.unlock();
    }
    else
    {
        errFlag = true;
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        std::string errMsg =
            "updateOrderStatsOnCxlOrderRejectFromMiddleSystem: order id not in order pos cache";
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {

#if defined(_PRINT_UPDATE)
        printOrderStats(resp.OrderID);
#endif
    }
}

void RmsImpl::handleCxlOrderReject(const ResponseMsg &msg)
{
    handleCxlOrderRejectFromMiddleSystem(msg);
}

/*!
 * incrementTradingStatsOnTrade
 */
void RmsImpl::handleTradeConfirm(const ResponseMsg &resp, const string &ticker, char side)
{
    double currentTradeValue = resp.Price * resp.Quantity;
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        if (!orderPos.confirmed) /*! adding this change for the case when order gets traded immediately and no confirmation is sent */
            orderPos.confirm();

        orderPos.openQty -= resp.Quantity;
        orderPos.doneQty += resp.Quantity;

        TickerPositions &tickPos = *(orderPos.tickerIterator->second);
        AccountPositions &accountPos = *(orderPos.accountIterator->second);
        tickPos.lock();
        if (illuminati::md::SIDE_BUY == orderPos.side)
        {
            if (orderPos.openQty <= 0)
            {
                tickPos.buyOpenOrdersCount--;
            }
            tickPos.netBuyOpenQty -= resp.Quantity;
            tickPos.netBuyTradedQty += resp.Quantity;
            tickPos.netBuyTradedValue += currentTradeValue;
            accountPos.updateBuyTradeQtyAndValue(resp.Quantity,
                                                 currentTradeValue);
        }
        else
        {
            if (orderPos.openQty <= 0)
            {
                tickPos.sellOpenOrdersCount--;
            }
            tickPos.netSellOpenQty -= resp.Quantity;
            tickPos.netSellTradedQty += resp.Quantity;
            tickPos.netSellTradedValue += currentTradeValue;
            accountPos.updateSellTradeQtyAndValue(resp.Quantity,
                                                  currentTradeValue);
        }
        tickPos.unlock();
        // update the total orders in a day stats for the associated account

        orderPos.accountIterator->second->ordersPerDay.updateTotalTradedValue(
            currentTradeValue);

        if (orderPos.openQty <= 0)
        {
            // We don't need to use sentQty delta anymore since we are not adding it too. That is taken care of by
            // modifySentButNotConfirmedQty

            /*if (orderPos.sentQtyDelta > 0)*/
            //{
            // tickPos.lock ();
            // if (illuminati::md::SIDE_BUY == orderPos.side)
            //{
            // tickPos.netBuyOpenQty -= orderPos.sentQtyDelta;
            //}
            // else
            //{
            // tickPos.netSellOpenQty -= orderPos.sentQtyDelta;
            //}
            // tickPos.unlock ();
            /*}*/

            // Handle the case where trade has completely filled the order but there is a pending modify
            // for which we haven't got an response yet. If we get modify rejects after this those will be
            // left unhandled as we don't have anything in the map
            tickPos.lock();
            if (illuminati::md::SIDE_BUY == orderPos.side)
            {
                // Clear the pending quantities from tickpositions
                tickPos.netBuyModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQty;
            }
            else
            {
                // Clear the pending quantities from tickpositions
                tickPos.netSellModifySentButNotConfirmedQty -= orderPos.modifySentButNotConfirmedQty;
            }
            tickPos.unlock();

            orderPos.unlock();
            m_orderToPositions.erase(iter);
        }
        else
        {
            orderPos.unlock();
        }
    }
    else
    {
        /*! case when cancel confirmation came before trade confirmation */
#if defined(_PRINT_UPDATE)
        ILOG(WARN) << "TRADE confirm came after ORDER CANCELLATION\n";
#endif
        std::string account(resp.AccountID);
        AccountPositions *accPos = getAccountPosFromCache(account);

        if (accPos == NULL)
        {
            ILOG(FATAL) << "Could not find positions for account: " << account << "\n";
            EMIT_PARAM(RMS_WARNING, ResponseType::RMS_REJECT, 0, E_ORS_SESSION_ABORTED);
            m_criticalSection.unlock();
            return;
        }

        TickerPositions *tickPos = NULL;
        // This can cause an issue with NSE since symbol there is not resp.Symbol but
        // a combination of details in ContractDescription. But this case of
        // trade confirms coming after cancel confirms does
        // not arise in NSE
        accPos->m_tickerPositionslock.lock();
        auto iter = accPos->perTickerPositions.find(std::string(resp.Symbol));
        if (iter != accPos->perTickerPositions.end())
            tickPos = iter->second;
        accPos->m_tickerPositionslock.unlock();

        if (NULL != tickPos)
        {
            tickPos->lock();
            if (illuminati::md::SIDE_BUY == side)
            {
                tickPos->netBuyTradedQty += resp.Quantity;
                tickPos->netBuyTradedValue += currentTradeValue;
            }
            else
            {
                tickPos->netSellTradedQty += resp.Quantity;
                tickPos->netSellTradedValue += currentTradeValue;
            }
            tickPos->unlock();
            // A strategy will have only one trading account
            accPos->ordersPerDay.updateTotalTradedValue(currentTradeValue);
        }
        else
        {
            ILOG(FATAL) << "TickerPositions is null for StrategyId \n";
        }
    }
    m_criticalSection.unlock();
}
/*!
 * storeOrderStats
 */
void RmsImpl::storeOrderStats(OrderPositions &allPos, const RequestMsg &req)
{
    m_criticalSection.lock();
    m_orderToPositions.insert(make_pair(req.OrderID, &allPos));
    m_criticalSection.unlock();
}
/*!
 * checkIfOrderExists
 */
bool RmsImpl::checkIfOrderExists(const RequestMsg &req)
{
    bool retVal;
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(req.OrderID);
    if (iter == m_orderToPositions.end())
    {
        retVal = false;
    }
    else
    {
        retVal = true;
    }
    m_criticalSection.unlock();
    return retVal;
}
/*!
 * getLastRequestOutgoingState
 */
illuminati::infra::RequestType RmsImpl::getLastRequestOutgoingState(
    const illuminati::infra::ResponseMsg &resp)
{
    OrderPositions *pOrderPos = getOrderPositions(resp.OrderID);
    if (pOrderPos != NULL)
    {
        return pOrderPos->lastRequestOutgoingRequest;
    }
    else
    {
        ILOG(FATAL) << "getLastRequestOutgoingState::order positions is null\n";
        return RequestType::NEWORDER;
    }
}
/*!
 * getOrderPositions
 */
OrderPositions *RmsImpl::getOrderPositions(uint32_t orderId)
{
    OrderPositions *retVal = NULL;
    auto iter = m_orderToPositions.find(orderId);
    if (iter != m_orderToPositions.end())
    {
        retVal = iter->second;
    }
    return retVal;
}

/*!
 * checkAndUpdateTradingStatsOnCancel
 */
bool RmsImpl::checkAndUpdateTradingStatsOnCancel(const RequestMsg &req,
                                                 int32_t &error, ResponseType &response)
{
    bool noError = true;
    m_criticalSection.lock();
    OrderPositions *orderPos = getOrderPositions(req.OrderID);
    if (NULL == orderPos)
    {
        noError = false;
        error = E_RMS_CANCEL_ORDER_NOT_FOUND;
        response = ResponseType::CANCEL_ORDER_REJECT;
    }

    if (noError)
    {
        orderPos->orderTime = req.TimeStamp;

        CancelCheckEnabled flag = NO_CHECK_ENABLED;
        if (m_orderFrequencyEnabled)
            flag = (CancelCheckEnabled)(flag | ORDER_FREQ_CXL_CHECK);
        if (m_cancelConfirmsCountCheckEnabled)
            flag = (CancelCheckEnabled)(flag | CONFIRM_COUNT_CXL_CHECK);

        switch (flag)
        {
        case NO_CHECK_ENABLED:
        {
            orderPos->lastRequestOutgoingRequest = RequestType::CANCELORDER;
            // increment the total order per day stats for the account
            orderPos->accountIterator->second->ordersPerDay.incrementCxlOrdersCount();
            orderPos->tickerIterator->second->lock();
            orderPos->tickerIterator->second->totalCancelOrderConfirms++;
            orderPos->tickerIterator->second->unlock();
            break;
        }
        case ORDER_FREQ_CHECK_ENABLED:
        {
            if (req.Exchange_Type == SGX)
            {
                noError = true;
            }
            else
            {
                noError = m_rmsChecks.execute(req, *orderPos, error, ORDER_FREQUENCY_CHECK);
            }

            if (noError)
            {
                updateOrderFrequency(orderPos->ordersPerSecond,
                                     orderPos->orderTime);
                orderPos->lastRequestOutgoingRequest =
                    RequestType::CANCELORDER;
                // increment the total order per day stats for the account
                orderPos->accountIterator->second->ordersPerDay.incrementCxlOrdersCount();
                orderPos->tickerIterator->second->lock();
                orderPos->tickerIterator->second->totalCancelOrderConfirms++;
                orderPos->tickerIterator->second->unlock();
            }
            else
            {
                response = ResponseType::RMS_REJECT;
            }
            break;
        }
        case CONFIRM_CXL_COUNT_CHECK_ENABLED:
        {
            noError = m_rmsChecks.execute(req, *orderPos, error,
                                          CANCEL_CONFIRMS_COUNT_CHECK);
            if (noError)
            {
                orderPos->lastRequestOutgoingRequest =
                    RequestType::CANCELORDER;
                // increment the total order per day stats for the account
                orderPos->accountIterator->second->ordersPerDay.incrementCxlOrdersCount();
                orderPos->tickerIterator->second->lock();
                orderPos->tickerIterator->second->totalCancelOrderConfirms++;
                orderPos->tickerIterator->second->unlock();
            }
            else
            {
                response = ResponseType::RMS_REJECT;
            }
            break;
        }
        case ALL_CHECK_ENABLED:
        {
            noError = m_rmsChecks.execute(req, *orderPos, error,
                                          ORDER_FREQUENCY_CHECK);
            if (noError)
            {
                noError = m_rmsChecks.execute(req, *orderPos, error,
                                              CANCEL_CONFIRMS_COUNT_CHECK);
                if (noError)
                {
                    updateOrderFrequency(orderPos->ordersPerSecond,
                                         orderPos->orderTime);
                    orderPos->lastRequestOutgoingRequest =
                        RequestType::CANCELORDER;
                    // increment the total order per day stats for the account
                    orderPos->accountIterator->second->ordersPerDay.incrementCxlOrdersCount();
                    orderPos->tickerIterator->second->lock();
                    orderPos->tickerIterator->second->totalCancelOrderConfirms++;
                    orderPos->tickerIterator->second->unlock();
                }
                else
                {
                    response = ResponseType::RMS_REJECT;
                }
            }
            else
            {
                response = ResponseType::RMS_REJECT;
            }
            break;
        }
        }
    }
    m_criticalSection.unlock();

    return noError;
}
/*!
 * cleanupRmsImpl
 */
void RmsImpl::cleanupRmsImpl()
{
    for (auto it = m_tickerToLTUpdates.begin();
         it != m_tickerToLTUpdates.end(); it++)
    {
        if (it->second != NULL)
            delete it->second;
    }
    for (auto it = m_accountPosStartUpCache.begin();
         it != m_accountPosStartUpCache.end(); it++)
    {
        if (it->second != NULL)
            delete it->second;
    }
    if (m_tickerToLTUpdates.size())
        m_tickerToLTUpdates.clear();

    if (m_orderToPositions.size())
        m_orderToPositions.clear();
}
/*!
 * printOrderStats
 */
void RmsImpl::printOrderStats(const uint32_t orderId)
{
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(orderId);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &pos = *(iter->second);
        ILOG(INFO) << pos.toString();
    }
    else
    {
        ILOG(INFO) << "NO positions for or affected by this order id = " << orderId << endl;
    }
    m_criticalSection.unlock();
}
/*!
 * printAllPositions
 */
void RmsImpl::printAllPositions()
{
    for (auto it = m_accountPosStartUpCache.begin(); it != m_accountPosStartUpCache.end(); ++it)
    {
        ILOG(INFO) << it->second->toString();
    }

    if (m_orderFrequency)
        ILOG(INFO) << m_orderFrequency->toString();
}

/*!
 * setAccountValuesInCorrectFormat
 * helper method
 */
void RmsImpl::setAccountValuesInCorrectFormat(const std::string &input,
                                              std::string &output, int32_t len)
{
    bool copy = true;
    for (int32_t i = 0; i < len; i++)
    {
        if (i >= input.size())
        {
            copy = false;
        }

        if (copy)
        {
            output += input[i];
        }
        else
        {
            output += ' ';
        }
    }
}

/*
 * order response
 */
void RmsImpl::handleORSResponse(ResponseMsg *respMsg)
{
#if defined(_RMS_STATS)
    uint64_t startTime = illuminati::ITime_ClockRT::GetCurrentTime();
#endif

    switch (respMsg->Response_Type)
    {
    case NEW_ORDER_CONFIRM:
    {
        handleNewOrderConfirm(*respMsg);
        break;
    }
    case TRADE_CONFIRM:
    {
        handleTradeConfirm(*respMsg, respMsg->Symbol, respMsg->Side);
        break;
    }
    case CANCEL_ORDER_REJECT:
    {
        if (respMsg->Child_Response == CANCEL_ORDER_REJECT_MIDDLE)
            handleCxlOrderRejectFromMiddleSystem(*respMsg);

        break;
    }
    case CANCEL_ORDER_CONFIRM:
    {
        handleCxlOrderConfirm(*respMsg);
        break;
    }
    case MODIFY_ORDER_CONFIRM:
    {
        handleModifyConfirm(*respMsg);
        break;
    }
    case MODIFY_ORDER_REJECT:
    {
        switch (respMsg->Child_Response)
        {
        case NULL_RESPONSE_MIDDLE:
        {
            handleModifyOrderReject(*respMsg);
            break;
        }
        case MODIFY_REJECT_MIDDLE:
        {
            handleModifyOrderRejectFromMiddleSystem(*respMsg);
            break;
        }
        }

        break;
    }
    case ORDER_ERROR:
    {
        switch (respMsg->Child_Response)
        {
        case NULL_RESPONSE_MIDDLE:
        {
            handleOrderReject(*respMsg);
            break;
        }
        case ORDER_REJECT_MIDDLE:
        {
            handleOrderRejectFromMiddleSystem(*respMsg);
            break;
        }
        }

        break;
    }
    default:
    {
        break;
    }
    }

#if defined(_PRINT_UPDATE)
    printAllPositions();
#endif

#if defined(_RMS_STATS)
    uint64_t timeTaken = illuminati::ITime_ClockRT::GetCurrentTime() - startTime;
    StatsRecorder::record(m_rmsStatResponse, timeTaken);
#endif
}
/*
 * handle md updates
 */
void RmsImpl::handleMDUpdates(MarketUpdateNew *update)
{
    updateLTPFromMD(update);
}
/*!
 * order request thread
 */
template <typename SYMBOL_NAMING_POLICY>
bool RmsImpl::handleORSRequests(RequestMsg *reqMsg)
{
#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "RecvdMsg: " << reqMsg->toString() << "\n";
#endif

    uint64_t startTime = illuminati::ITime_ClockRT::GetCurrentTime();

    bool retVal = false;
    switch (reqMsg->Request_Type)
    {
    case NEWORDER:
    {
        retVal = executeNewOrderChecks<SYMBOL_NAMING_POLICY>(*reqMsg);
#ifdef _RMS_STATS
        StatsRecorder::record(NEW_ORDER_ONE, illuminati::ITime_ClockRT::GetCurrentTime() - startTime);
#endif
        break;
    }
    case MODIFYORDER:
    {
        retVal = executeModifyOrderChecks<SYMBOL_NAMING_POLICY>(*reqMsg);
#ifdef _RMS_STATS
        StatsRecorder::record(MODIFY_ORDER_ONE, illuminati::ITime_ClockRT::GetCurrentTime() - startTime);
#endif
        break;
    }
    case CANCELORDER:
    {
        retVal = executeCancelOrderChecks<SYMBOL_NAMING_POLICY>(*reqMsg);
#ifdef _RMS_STATS
        StatsRecorder::record(CANCEL_ORDER_ONE, illuminati::ITime_ClockRT::GetCurrentTime() - startTime);
#endif
        break;
    }
    }
    return retVal;
}
#endif

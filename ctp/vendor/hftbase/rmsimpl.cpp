#include <string>
#include <cassert>
#include "rmsimpl.h"
#include "logger.h"
#include "constants.h"
#include "orderresponse.h"
#include "rmsloginstruct.h"

using namespace std;
using namespace illuminati::md;
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

    m_rmsConfigFile = rmsConfig;
    m_rmsFirmwideConfigFile = rmsFirmwideConfig;
    m_rmsFirmwideConfigKey = rmsFirmiwdeConfigKey;
    m_totalPositions.reserve(maxShmClients);
    for (int32_t i = 0; i < maxShmClients; i++)
    {
        m_totalPositions.push_back(NULL);
    }

    reset();
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
    /// firm account level container which checks for total orders sent per second.
    if (m_orderFrequencyEnabled)
    {
        m_orderFrequency = new OrderFrequency(m_orderPerSecondLimit);
    }
    else
    {
        m_orderFrequency = new OrderFrequency(1);
    }

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
    }

    /*
     illuminati::Configfile& cfg =  illuminati::Configfile::GetInstance();
     cfg.LoadCfg(m_rmsConfigFile);


     std::vector<std::string> eligibleAccounts = cfg.getAllSections();
     for(auto it = eligibleAccounts.begin(); it != eligibleAccounts.end(); it++)
     {
     std::string accountid;
     setAccountValuesInCorrectFormat(illuminati::trim(*it), accountid, MAX_ACCNTID_LEN);

     AccountPositions* pAcc = NULL;
     auto accCacheIt =  m_accountPosStartUpCache.find(accountid);
     if (m_accountPosStartUpCache.end() == accCacheIt)
     {
     pAcc = new AccountPositions(accountid);
     m_accountPosStartUpCache.insert(make_pair(accountid, pAcc));
     }
     else
     {
     pAcc = accCacheIt->second;
     }
     pAcc->thresholds.singleOrderOpenQtyLimit     = cfg.getInt64_s((*it).c_str(), "SINGLE_ORDER_OPEN_QTY_THRESHOLD", 0);
     pAcc->thresholds.singleOrderOpenValueLimit   = cfg.getInt64_s((*it).c_str(),"SINGLE_ORDER_OPEN_VALUE_THRESHOLD", 0);
     pAcc->thresholds.singleTickerOpenOrdersLimit = cfg.getInt64_s((*it).c_str(), "PER_TICKER_OPEN_ORDERS_LIMIT", 1);
     pAcc->thresholds.singleTickerNetValueLimit   = cfg.getInt64_s((*it).c_str(),"PER_TICKER_NET_VALUE_LIMIT", 1);
     pAcc->thresholds.m_checkForExposure          = cfg.getBoolStrict((*it).c_str(),"CHECK_FOR_EXPOSURE");
     pAcc->thresholds.m_ordersPerDayFixedLimit    = cfg.getInt64_s((*it).c_str(),"ORDERS_PER_DAY_FIXED_LIMIT",0);
     pAcc->thresholds.m_tradedValueMultiplyFactor = cfg.getDouble((*it).c_str(),"TRADED_VALUE_MULTIPLY_FACTOR", 0.0);
     pAcc->thresholds.m_orderToCommissionFactor   = cfg.getDouble((*it).c_str(),"ORDER_TO_COMMISSION_FACTOR", 0.0);
     pAcc->thresholds.m_orderPerDayLowerLimit     = cfg.getDouble((*it).c_str(),"ORDERS_PER_DAY_LOWER_LIMIT", 0.0);
     pAcc->thresholds.m_orderPerDayUpperLimit     = cfg.getDouble((*it).c_str(),"ORDERS_PER_DAY_UPPER_LIMIT", 0.0);
     pAcc->thresholds.m_orderPerTradeLimit        = cfg.getInt64_s((*it).c_str(),"ORDERS_PER_TRADE_LIMIT",0);
     pAcc->thresholds.m_orderPerTradeLowerLimit   = cfg.getDouble((*it).c_str(),"ORDERS_PER_TRADE_LOWER_LIMIT", 0.0);
     pAcc->thresholds.m_orderPerTradeUpperLimit   = cfg.getDouble((*it).c_str(),"ORDERS_PER_TRADE_UPPER_LIMIT", 0.0);
     }
     */
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
TickerPositions *RmsImpl::getTickerPosFromCache(string &ticker)
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
bool RmsImpl::executeNewOrderChecks(const illuminati::infra::RequestMsg &req)
{
    int32_t rmsError = E_RMS_NO_ERROR;
    OrderPositions *totalPos = getTradingStatSoFar(req);
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
        if (rmsError == E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_BREACHED)
        {
            EMIT_PARAM(RMS_ERROR, ResponseType::ORDERS_PER_DAY_LIMIT_REJECT, req.OrderID, rmsError);
        }
        else
        {
            EMIT_PARAM(RMS_ERROR, ResponseType::RMS_REJECT, req.OrderID, rmsError);
        }
        printAllPositions(req);
    }
    else
    {
        if (rmsError == E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_WARNING)
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
bool RmsImpl::executeModifyOrderChecks(const RequestMsg &req)
{
    int32_t rmsError = E_RMS_NO_ERROR;
    OrderPositions *totalPos = getTradingStatSoFar(req);
    if (NULL == totalPos)
    {
        rmsError = E_RMS_UNEXPECTED_ORDERID;
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        EMIT_PARAM(RMS_ERROR,
                   ResponseType::MODIFY_ORDER_REJECT, req.OrderID, rmsError);
        return false;
    }

    bool bRmsCheckSuccess = m_rmsChecks.execute(req, *totalPos, rmsError);
    if (!bRmsCheckSuccess)
    {
        ILOG(FATAL) << "RmsErrorCode: " << rmsError << " Desc:" << getErrorCodeDesciption(rmsError) << " RequestDetails:" << req.toStringLine() << "\n";
        EMIT_PARAM(RMS_ERROR, ResponseType::RMS_REJECT, req.OrderID, rmsError);
        printAllPositions(req);
    }
    else
    {
        if (rmsError == E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::STOP_LOSS_WARNING, req.OrderID, rmsError);
        }
        else if (rmsError == E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING)
        {
            EMIT_PARAM(RMS_WARNING, ResponseType::STOP_LOSS_WARNING, req.OrderID, rmsError);
        }

        incrementOrDecrementTradingStatsOnModifyOrder(*totalPos, req);
    }
    return bRmsCheckSuccess;
}
/*!
 * executeCancelOrderChecks
 */
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
        printAllPositions(req);
    }
    return bRmsCheckSuccess;
}

/*!
 * getTradingStatSoFar -- get called from the Request thread
 * Will return NULL if the req.AccountID is not configured in the RMS config file
 */
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
    int32_t stratId = req.OrderID / 1000000; // 1Million
    string accountId = req.AccountID;
    string ticker = req.Contract_Description.Symbol; // or expose a getSymbolMethod
    OrderPositions *totalPos = new OrderPositions(req.OrderID);

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

    StrategyPositions *positionsOnStrat = m_totalPositions[stratId];
    if (positionsOnStrat == NULL)
    {
        positionsOnStrat = new StrategyPositions(stratId);
        m_totalPositions[stratId] = positionsOnStrat;
    }
    totalPos->stratPosition = positionsOnStrat;

    auto accniter = positionsOnStrat->perAccountPositions.find(accountId);
    if (accniter == positionsOnStrat->perAccountPositions.end())
    {
        AccountPositions *newAccount = getAccountPosFromCache(accountId);
        if (NULL == newAccount)
        {
            m_rmsStateError = E_RMS_BLOCKED_ACCOUNT;
            ILOG(FATAL) << "account " << accountId << " is not configured in the RMS config \n";
            return NULL;
        }
        auto ret = positionsOnStrat->perAccountPositions.insert(
            pair<string, AccountPositions *>(accountId, newAccount));
        accniter = ret.first; /*! assuming insert always succeeds thats why no check for ret.second == true */
    }
    AccountPositions &accountPositions = *(accniter->second);
    totalPos->accountIterator = accniter;
    /// strategy position object holds the map of account but a strategy only has only one account to trade.
    /// hence adding this account iterator to the begin iterator of the map explicitly
    if (positionsOnStrat->perAccountPositions.size() > 1)
    { /// have this for few days of testing
        m_rmsStateError = E_RMS_STRAT_WITH_MORE_ACCOUNTS;
        ILOG(FATAL) << "More than two trading account with strategy \n";
        return NULL;
    }

    positionsOnStrat->accountIterator =
        positionsOnStrat->perAccountPositions.begin();

    auto tickiter = accountPositions.perTickerPositions.find(ticker);
    if (tickiter == accountPositions.perTickerPositions.end())
    {
        TickerPositions *tickFromCache = getTickerPosFromCache(ticker);
        if (NULL == tickFromCache)
        {
            m_rmsStateError = E_RMS_BLOCKED_SYMBOL;
            ILOG(FATAL) << "symbol " << ticker << " is not found in instrument cache \n";
            return NULL;
        }
        TickerPositions *newTicker = new TickerPositions(tickFromCache);
        newTicker->thresholds =
            accountPositions.m_accountLimits->getInstrumentLimits(ticker);
        // ILOG(INFO)<<"Thresholds for "<<ticker<<":\n";
        // newTicker->thresholds->printLimits();

        auto ret = accountPositions.perTickerPositions.insert(
            pair<string, TickerPositions *>(ticker, newTicker));
        tickiter = ret.first; /*! assuming insert always succeeds thats why no check for ret.second == true */
    }
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

    return totalPos;
}
/*!
 * decrementTradingStatsOnCxlOrderConfirm
 */
void RmsImpl::decrementTradingStatsOnCxlOrderConfirm(const ResponseMsg &resp)
{
    bool errFlag = false;
    std::string errMsg = "";
    double orderQty = 0.0;
    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter == m_orderToPositions.end())
    {
        errFlag = true;
        std::ostringstream css;
        css << "NO matching order in cache for this orderid " << resp.OrderID
            << " though we are in cxl confirm\n";
        errMsg = css.str();
    }
    else
    {
        OrderPositions &pos = *(iter->second);
        pos.lock();
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
        }
        else
        {
            if (pos.openQty <= 0)
            {
                tickerPos.sellOpenOrdersCount--;
            }
            tickerPos.netSellOpenQty -= orderQty;
        }
        tickerPos.unlock();
        if (pos.openQty <= 0)
        {
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
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
        //          printOrderStats (resp.OrderID);
        //          printAllPositions(resp.OrderID/1000000);
    }
}
/*!
 * updateOrderFrequency
 */
void RmsImpl::updateOrderFrequency(OrderFrequency *orderFrequency,
                                   uint64_t orderTime)
{
    if (orderFrequency->m_orderQueue.isFull())
    {
        orderFrequency->m_orderQueue.dequeue();
    }
    orderFrequency->m_orderQueue.enqueue(orderTime);
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
    //     printOrderStats (req.OrderID);
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
    if (req.Quantity > orderPos.openQty)
    {
        /// MODIFY is done to increase the quantity
        if (illuminati::md::SIDE_BUY == req.Transaction_Type)
        {
            tickPos.netBuyOpenQty += (req.Quantity - orderPos.openQty);
        }
        else if (illuminati::md::SIDE_SELL == req.Transaction_Type)
        {
            tickPos.netSellOpenQty += (req.Quantity - orderPos.openQty);
        }
    }
    else
    {
        /// MODIFY done to decrease the quantity has no effect
    }
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
    //     printOrderStats (req.OrderID);
}
/*!
 * updateOrderStatsOnNewOrder
 */
void RmsImpl::updateOrderStatsOnNewOrder(const ResponseMsg &resp)
{
    bool errFlag = false;
    std::string errMsg;

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
        errMsg = "updateOrderStatsOnNewOrder: order id not in order pos cache";
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
        //          printOrderStats (resp.OrderID);
    }
}
/*!
 * updateOrderStatsOnOrderModify
 */
void RmsImpl::updateOrderStatsOnOrderModify(const ResponseMsg &resp)
{
    bool errFlag = false;
    std::string errMsg;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        /// update order's openQty with sentQtyDelta
        orderPos.openQty = orderPos.openQty + orderPos.sentQtyDelta;
        if (orderPos.sentQtyDelta < 0)
        {
            TickerPositions &tickerPos = *(orderPos.tickerIterator->second);
            tickerPos.lock();
            if (illuminati::md::SIDE_BUY == orderPos.side)
            {
                if (orderPos.openQty <= 0)
                {
                    tickerPos.buyOpenOrdersCount--;
                }
                tickerPos.netBuyOpenQty += orderPos.sentQtyDelta;
            }
            else
            {
                if (orderPos.openQty <= 0)
                {
                    tickerPos.sellOpenOrdersCount--;
                }
                tickerPos.netSellOpenQty += orderPos.sentQtyDelta;
            }
            tickerPos.unlock();
        }
        orderPos.sentQty = 0;
        orderPos.sentQtyDelta = 0;

        if (orderPos.openQty <= 0)
        {
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
        errMsg = "updateOrderStatsOnNewOrder: order id not in order pos cache";
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
        //          printOrderStats (resp.OrderID);
    }
}
/*!
 * updateOrderStatsOnOrderReject
 */
void RmsImpl::updateOrderStatsOnOrderReject(const ResponseMsg &resp)
{
    bool errFlag = false;
    std::string errMsg;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
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
    }
    else
    {
        errFlag = true;
        errMsg =
            "updateOrderStatsOnOrderReject: order id not in order pos cache";
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
        //          printOrderStats (resp.OrderID);
    }
}
/*!
 * updateOrderStatsOnModifyOrderReject
 */
void RmsImpl::updateOrderStatsOnModifyOrderReject(const ResponseMsg &resp)
{
    bool errFlag = false;
    std::string errMsg;

    m_criticalSection.lock();
    auto iter = m_orderToPositions.find(resp.OrderID);
    if (iter != m_orderToPositions.end())
    {
        OrderPositions &orderPos = *(iter->second);
        orderPos.lock();
        /// if the MODIFY is done to increase the qty then only decrement Ticker's open qty.
        if (orderPos.sentQtyDelta > 0)
        {
            TickerPositions &tickPos = *(orderPos.tickerIterator->second);
            tickPos.lock();
            if (illuminati::md::SIDE_BUY == orderPos.side)
            {
                tickPos.netBuyOpenQty -= orderPos.sentQtyDelta;
            }
            else
            {
                tickPos.netSellOpenQty -= orderPos.sentQtyDelta;
            }
            tickPos.unlock();
        }
        orderPos.sentQty = 0;
        orderPos.sentQtyDelta = 0;
        orderPos.unlock();
    }
    else
    {
        errFlag = true;
        errMsg =
            "updateOrderStatsOnOrderReject: order id not in order pos cache";
    }
    m_criticalSection.unlock();

    if (errFlag)
    {
        ILOG(FATAL) << errMsg << endl;
    }
    else
    {
        //          printOrderStats (resp.OrderID);
    }
}
/*!
 * incrementTradingStatsOnTrade
 */
void RmsImpl::incrementTradingStatsOnTrade(const ResponseMsg &resp,
                                           const string &ticker, char side)
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

        if (orderPos.openQty == 0)
        {
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
        // ILOG(INFO) << "TRADE confirm came after ORDER CANCELLATION\n";
        int32_t stratId = resp.OrderID / 1000000;
        if (NULL != m_totalPositions[stratId])
        {
            TickerPositions *tickPos =
                m_totalPositions[stratId]->findTickerPositions(ticker);
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
                // TODO: how to update the totaltrade value for the account?
                // as accountid  is missing here
                m_totalPositions[stratId]->accountIterator->second->ordersPerDay.updateTotalTradedValue(
                    currentTradeValue);
                // m_totalOrdersPerDay->updateTotalTradedValue(currentTradeValue);
            }
            else
            {
                ILOG(FATAL) << "TickerPositions is null for StrategyId " << stratId << "\n";
            }
        }
        else
        {
            ILOG(FATAL) << "StrategyId " << stratId << " not found\n";
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

    if (noError && m_orderFrequencyEnabled)
    {
        orderPos->orderTime = req.TimeStamp;
        noError = m_rmsChecks.execute(req, *orderPos, error,
                                      ORDER_FREQUENCY_CHECK);
        if (noError)
        {
            updateOrderFrequency(orderPos->ordersPerSecond,
                                 orderPos->orderTime);
            orderPos->lastRequestOutgoingRequest = RequestType::CANCELORDER;
        }
        else
        {
            response = ResponseType::RMS_REJECT;
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
    for (auto it = m_totalPositions.begin(); it != m_totalPositions.end();
         it++)
    {
        if (*it != NULL)
            delete *it;
    }
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
void RmsImpl::printAllPositions(const RequestMsg &req)
{
    int stratId = req.OrderID / 1000000;
    if (m_totalPositions[stratId] != NULL)
        ILOG(INFO) << m_totalPositions[stratId]->toString();
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

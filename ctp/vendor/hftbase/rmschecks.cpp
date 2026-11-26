#include <assert.h>
#include "rmschecks.h"
#include "logger.h"
#include "errorcodes.h"
#include "constants.h"
#include "txncostcalculators.h"
#include <vector>
#include <cmath>
#include <iomanip>

using namespace std;
using namespace illuminati::infra;

/*!
 * RmsChecks
 */
RmsChecks::RmsChecks()
{
    m_accountPosStartUpCache = NULL;
    m_instrDefCache = NULL;
    m_checkForOrderFrequency = false;
    m_orderFrequencyLimit = 0;
    m_orderFrequencyWindow = 0.0;
    m_pThresholds = NULL;
    m_instrDet = NULL;
    m_cancelConfirmsCountCheckEnabled = false;
}
/*!
 * reset
 */
void RmsChecks::reset(RegisteredRMSAccountPosCache &accountPosStartUpCache,
                      RMSInstrumentCache &instrDefCache, RmsConfig rmsConfig)
{
    m_rmsConfig = rmsConfig;
    m_accountPosStartUpCache = &accountPosStartUpCache;
    m_instrDefCache = &instrDefCache;

    memset(m_AllRules, '\0', sizeof(m_AllRules));
    m_banndedSymbols.clear();
    m_subscribedRules.clear();

    // Get the allowed and banned symbols.
    m_banndedSymbols = m_rmsConfig.getBannedSymbols();
    m_allowedSymbols = m_rmsConfig.getEnabledSymbols();
    vector<string> rules = m_rmsConfig.getEnabledChecks();

    /*
     illuminati::Configfile& cfg = illuminati::Configfile::GetInstance();

     vector<string> bannedSymbs = illuminati::stringtokens(cfg.getString("BANNED_SYMBOLS"), ',');
     vector<string> rules = illuminati::stringtokens(cfg.getString("CHECK_LIST"), ',');
     if (rules.size() > MAX_RMS_RULES)
     {
     ILOG(FATAL) << "max rms rules exceeded.size=" << rules.size() << " maxallowed="<< MAX_RMS_RULES << "\n";
     cout        << "max rms rules exceeded.size=" << rules.size() << " maxallowed="<< MAX_RMS_RULES << "\n";
     throw("");
     }

     for(auto it = bannedSymbs.begin(); it != bannedSymbs.end(); it++)
     {
     m_banndedSymbols.insert(illuminati::trim(*it));
     }
     */

    // load the rules
    for (auto it = rules.begin(); it != rules.end(); it++)
    {
        if (illuminati::trim(*it) == "VALID_SYMBOL_CHECK")
        {
            m_subscribedRules.push_back(VALID_SYMBOL_CHECK);
        }
        else if (illuminati::trim(*it) == "ORDER_LIMIT_CHECK")
        {
            m_subscribedRules.push_back(ORDER_LIMIT_CHECK);
        }
        else if (illuminati::trim(*it) == "TICKER_LIMIT_CHECK")
        {
            m_subscribedRules.push_back(TICKER_LIMIT_CHECK);
        }
        else if (illuminati::trim(*it) == "ORDER_FREQUENCY_CHECK")
        {
            m_subscribedRules.push_back(ORDER_FREQUENCY_CHECK);
            m_checkForOrderFrequency = true;
            m_orderFrequencyLimit = m_rmsConfig.getFirmwideLimits()->m_orderFrequencyLimit;
            m_orderFrequencyWindow = m_rmsConfig.getFirmwideLimits()->m_orderFrequencyWindow * NANO_SECONDS;
            m_orderFreqCalcMode = m_rmsConfig.getFirmwideLimits()->m_orderFreqCalcMode;
        }
        else if (illuminati::trim(*it) == "NEGATIVE_PRICE_OR_QUANTITY_CHECK")
        {
            m_subscribedRules.push_back(NEGATIVE_PRICE_OR_QUANTITY_CHECK);
        }
        else if (illuminati::trim(*it) == "ORDERS_PER_DAY_CHECK")
        {
            m_subscribedRules.push_back(ORDERS_PER_DAY_CHECK);
        }
        else if (illuminati::trim(*it) == "ORDERS_PER_TRADE_CHECK")
        {
            m_subscribedRules.push_back(ORDERS_PER_TRADE_CHECK);
        }

        else if (illuminati::trim(*it) == "QUANTITY_FIRMLEVEL_CHECK")
        {
            m_subscribedRules.push_back(QUANTITY_FIRMLEVEL_CHECK);
        }
        else if (illuminati::trim(*it) == "NET_POSITION_FIRMLEVEL_CHECK")
        {
            m_subscribedRules.push_back(NET_POSITION_FIRMLEVEL_CHECK);
        }
        else if (illuminati::trim(*it) == "ALLOWED_SYMBOL_CHECK")
        {
            m_subscribedRules.push_back(ALLOWED_SYMBOL_CHECK);
        }
        else if (illuminati::trim(*it) == "STOP_LOSS_CHECK")
        {
            m_subscribedRules.push_back(STOP_LOSS_CHECK);
        }
        else if (illuminati::trim(*it) == "KRX_ORDERS_PER_TRADE_CHECK")
        {
            m_subscribedRules.push_back(KRX_ORDERS_PER_TRADE_CHECK);
        }
        else if (illuminati::trim(*it) == "X_PERCENT_AWAY_FROM_LTP_CHECK")
        {
            m_subscribedRules.push_back(X_PERCENT_AWAY_FROM_LTP_CHECK);
        }
        else if (illuminati::trim(*it) == "CANCEL_CONFIRMS_COUNT_CHECK")
        {
            m_cancelConfirmsCountCheckEnabled = true;
            m_subscribedRules.push_back(CANCEL_CONFIRMS_COUNT_CHECK);
        }
    }

    ILOG(INFO) << "number of subscribed rules " << m_subscribedRules.size() << std::endl;

    int32_t subscribedRulesCount = 0;

    for (auto it = m_subscribedRules.begin(); it != m_subscribedRules.end();
         it++)
    {
        switch (*it)
        {
        case VALID_SYMBOL_CHECK:
        {
            m_AllRules[VALID_SYMBOL_CHECK] = std::bind(&RmsChecks::IsSymbolAllowed, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case ORDER_LIMIT_CHECK:
        {
            m_AllRules[ORDER_LIMIT_CHECK] = std::bind(&RmsChecks::SingleOrderThresholdChecks, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case TICKER_LIMIT_CHECK:
        {
            m_AllRules[TICKER_LIMIT_CHECK] = std::bind(&RmsChecks::SingleTickerThresholdChecks, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case ORDER_FREQUENCY_CHECK:
        {
            m_AllRules[ORDER_FREQUENCY_CHECK] = std::bind(&RmsChecks::OrderFrequencyThresholdChecks, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case ORDERS_PER_DAY_CHECK:
        {
            m_AllRules[ORDERS_PER_DAY_CHECK] = std::bind(&RmsChecks::OrdersPerDayThresholdChecks, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case ORDERS_PER_TRADE_CHECK:
        {
            m_AllRules[ORDERS_PER_TRADE_CHECK] = std::bind(&RmsChecks::OrdersPerTradeThresholdChecks, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }

        case ALLOWED_SYMBOL_CHECK:
        {
            m_AllRules[ALLOWED_SYMBOL_CHECK] = std::bind(&RmsChecks::IsSymbolEnabled, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case QUANTITY_FIRMLEVEL_CHECK:
        {
            m_AllRules[QUANTITY_FIRMLEVEL_CHECK] = std::bind(&RmsChecks::QuantityFirmLevelCheck, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case NET_POSITION_FIRMLEVEL_CHECK:
        {
            m_AllRules[NET_POSITION_FIRMLEVEL_CHECK] = std::bind(&RmsChecks::NetPositionFirmLevelCheck, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case STOP_LOSS_CHECK:
        {
            m_AllRules[STOP_LOSS_CHECK] = std::bind(&RmsChecks::StopLossCheck, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case X_PERCENT_AWAY_FROM_LTP_CHECK:
        {
            m_AllRules[X_PERCENT_AWAY_FROM_LTP_CHECK] = std::bind(
                &RmsChecks::XPercentAwayFromLTPCheck, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case KRX_ORDERS_PER_TRADE_CHECK:
        {
            m_AllRules[KRX_ORDERS_PER_TRADE_CHECK] = std::bind(&RmsChecks::KRXTotalOrdersPerTradeUsingTradedQtyChecks, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case CANCEL_CONFIRMS_COUNT_CHECK:
        {
            m_AllRules[CANCEL_CONFIRMS_COUNT_CHECK] = std::bind(&RmsChecks::CancelConfirmationCountCheck, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        case NEGATIVE_PRICE_OR_QUANTITY_CHECK:
        {
            m_AllRules[NEGATIVE_PRICE_OR_QUANTITY_CHECK] = std::bind(&RmsChecks::NegativePriceOrQuantityCheck, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            subscribedRulesCount++;
            break;
        }
        default:
        {
            break;
        }
        }
    }

    ILOG(INFO) << "total number of subscribed rules are " << subscribedRulesCount << " from file " << illuminati::Configfile::GetInstance().getConfigFileName() << "\n";
}
/*!
 * setCurrentInstrumentDetails
 */
void RmsChecks::setCurrentInstrumentDetails(const RequestMsg &req,
                                            OrderPositions &positions)
{
    m_instrDet = &(positions.tickerIterator->second->m_instrumentDetails);
}
/*!
 * setThresholdsForAccount, this also works as membership check
 */
bool RmsChecks::setThresholdsForAccount(const RequestMsg &req,
                                        OrderPositions &positions, int32_t &errorNum)
{
    bool retVal = true;
    // m_pThresholds = positions.accountIterator->second->;
    return retVal;
}

bool RmsChecks::setThresholdsForInstrument(const RequestMsg &req,
                                           OrderPositions &positions, int32_t &errorNum)
{
    bool retVal = true;
    m_pThresholds = positions.tickerIterator->second->thresholds;
    return retVal;
}
/*!
 * setCurrentValuesFromCaches
 */
bool RmsChecks::setCurrentValuesFromCaches(const RequestMsg &req,
                                           OrderPositions &positions, int32_t &errorNum)
{
    setCurrentInstrumentDetails(req, positions);
    bool retValue = setThresholdsForInstrument(req, positions, errorNum);
    return retValue;
}
/*!
 * rules execution
 */
bool RmsChecks::execute(const RequestMsg &req, OrderPositions &positions,
                        int32_t &errorNum)
{
    lock();
    errorNum = E_RMS_NO_ERROR;

    if (!setCurrentValuesFromCaches(req, positions, errorNum))
    {
        unlock();
        return false;
    }

    bool rulesPassed = true;
    for (int i = 0; i < m_subscribedRules.size(); i++)
    {
        if (!m_AllRules[m_subscribedRules[i]](req, positions, errorNum))
        {
            rulesPassed = false;
            break;
        }
    }

    unlock();
    return rulesPassed;
}
/*!
 * rules execution in case of a specific rule
 */
bool RmsChecks::execute(const RequestMsg &req, OrderPositions &positions,
                        int32_t &errorNum, RuleTypes ruleId)
{
    lock();
    errorNum = E_RMS_NO_ERROR;

    // load the limits for the incoming account, this works as membership check also
    if (!setCurrentValuesFromCaches(req, positions, errorNum))
    {
        unlock();
        return false;
    }

    bool rulesPassed = true;
    if (!m_AllRules[ruleId](req, positions, errorNum))
    {
        rulesPassed = false;
    }

    unlock();
    return rulesPassed;
}
/*!
 * symbol allowed rule
 */
bool RmsChecks::IsSymbolAllowed(const RequestMsg &req,
                                OrderPositions &positions, int32_t &errorNum)
{
    auto iter = m_banndedSymbols.find(std::string(req.Contract_Description.Symbol));

    if (iter != m_banndedSymbols.end())
    {
        errorNum = E_RMS_BLOCKED_SYMBOL;
        return false;
    }

    errorNum = E_RMS_NO_ERROR;
    return true;
}

bool RmsChecks::IsSymbolEnabled(const RequestMsg &req,
                                OrderPositions &positions, int32_t &errorNum)
{
    auto iter = m_allowedSymbols.find(std::string(req.Contract_Description.Symbol));
    if (iter == m_allowedSymbols.end())
    {
        errorNum = E_RMS_NOT_ALLOWED_SYMBOL;
        return false;
    }
    errorNum = E_RMS_NO_ERROR;
    return true;
}
/*!
 * apply threshold on single order values
 */
bool RmsChecks::SingleOrderThresholdChecks(const RequestMsg &req,
                                           OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = positions.tickerIterator->second->thresholds;

    if (req.Quantity > (m_pThresholds->singleOrderOpenQtyLimit * m_instrDet->RoundLotForThresholdCheck))
    {
        ILOG(FATAL) << "ORDER_LIMIT_CHECK failed" << std::endl;
        ILOG(FATAL) << "Error code = E_RMS_ORDER_QTY_LIMIT_BREACHED(" << E_RMS_ORDER_QTY_LIMIT_BREACHED << ")" << std::endl;
        ILOG(FATAL) << "Reason: req.Quantity = " << req.Quantity << " >= m_pThresholds->singleOrderOpenQtyLimit = " << m_pThresholds->singleOrderOpenQtyLimit << std::endl;
        errorNum = E_RMS_ORDER_QTY_LIMIT_BREACHED;
        return false;
    }

    double price;
    TickerLTPUpdates &ltpUpdate = *(positions.tickerIterator->second->tickLtpIterator->second);
    ltpUpdate.lock();
    price = (ltpUpdate.lastTradePrice) ? ltpUpdate.lastTradePrice : req.Price;
    ltpUpdate.unlock();

    if ((req.Quantity * m_instrDet->RoundLotForValueCalc * price * m_instrDet->PriceFactor) > m_pThresholds->singleOrderOpenValueLimit)
    {
        ILOG(FATAL) << "ORDER_LIMIT_CHECK failed" << std::endl;
        ILOG(FATAL) << "Error code = E_RMS_ORDER_VALUE_LIMIT_BREACHED(" << E_RMS_ORDER_VALUE_LIMIT_BREACHED << ")" << std::endl;
        ILOG(FATAL) << "Reason: orderValue = " << (req.Quantity * m_instrDet->RoundLotForValueCalc * price * m_instrDet->PriceFactor)
                    << " >= singleOrderValueLimit = " << m_pThresholds->singleOrderOpenValueLimit << std::endl;
        errorNum = E_RMS_ORDER_VALUE_LIMIT_BREACHED;
        return false;
    }

    errorNum = E_RMS_NO_ERROR;
    return true;
}
/*!
 * apply thresholds on single trading symbol/ticker values
 */
bool RmsChecks::SingleTickerThresholdChecks(const RequestMsg &req,
                                            OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = positions.tickerIterator->second->thresholds;
    double currentValue = 0.0;
    double currentQty = 0.0;
    double price = 0.0;
    bool noError = true;
    TickerPositions &tickPos = *(positions.tickerIterator->second);
    tickPos.lock();

    TickerLTPUpdates &ltpUpdate = *(tickPos.tickLtpIterator->second);
    ltpUpdate.lock();
    price = (ltpUpdate.lastTradePrice) ? ltpUpdate.lastTradePrice : req.Price;
    ltpUpdate.unlock();

    currentValue = (tickPos.netBuyTradedValue - tickPos.netSellTradedValue) * m_instrDet->RoundLotForValueCalc * m_instrDet->PriceFactor;

    if (RequestType::NEWORDER == req.Request_Type)
    {
        if (illuminati::md::SIDE_BUY == req.Transaction_Type)
        {
            if (tickPos.buyOpenOrdersCount + 1 > m_pThresholds->singleTickerOpenOrdersLimit)
            {
                ILOG(FATAL) << "TICKER_LIMIT_CHECK failed" << std::endl;
                ILOG(FATAL) << "Error code = E_RMS_TICKER_OO_LIMIT_BREACHED(" << E_RMS_TICKER_OO_LIMIT_BREACHED << ")" << std::endl;
                ILOG(FATAL) << "Reason: buyOpenOrdersCount = " << tickPos.buyOpenOrdersCount << " >= singleTickerOpenOrdersLimit = " << m_pThresholds->singleTickerOpenOrdersLimit << std::endl;
                errorNum = E_RMS_TICKER_OO_LIMIT_BREACHED;
                noError = false;
            }
            else
            {
                currentValue += (tickPos.netBuyOpenQty + tickPos.netBuyModifySentButNotConfirmedQty + req.Quantity) * m_instrDet->RoundLotForValueCalc * price * m_instrDet->PriceFactor;
            }
        }
        else
        {
            if (tickPos.sellOpenOrdersCount + 1 > m_pThresholds->singleTickerOpenOrdersLimit)
            {
                ILOG(FATAL) << "TICKER_LIMIT_CHECK failed" << std::endl;
                ILOG(FATAL) << "Error code = E_RMS_TICKER_OO_LIMIT_BREACHED(" << E_RMS_TICKER_OO_LIMIT_BREACHED << ")" << std::endl;
                ILOG(FATAL) << "Reason: sellOpenOrdersCount = " << tickPos.sellOpenOrdersCount << " >= singleTickerOpenOrdersLimit = " << m_pThresholds->singleTickerOpenOrdersLimit << std::endl;
                errorNum = E_RMS_TICKER_OO_LIMIT_BREACHED;
                noError = false;
            }
            else
            {
                currentValue += (0 - (req.Quantity + tickPos.netSellOpenQty + tickPos.netSellModifySentButNotConfirmedQty)) * m_instrDet->RoundLotForValueCalc * price * m_instrDet->PriceFactor;
            }
        }
    }
    else if (RequestType::MODIFYORDER == req.Request_Type)
    {
        if (illuminati::md::SIDE_BUY == req.Transaction_Type)
        {
            currentValue += (tickPos.netBuyOpenQty + tickPos.netBuyModifySentButNotConfirmedQty + req.Quantity) * m_instrDet->RoundLotForValueCalc * price * m_instrDet->PriceFactor;
        }
        else
        {
            currentValue += (0 - (tickPos.netSellOpenQty + tickPos.netSellModifySentButNotConfirmedQty + req.Quantity)) * m_instrDet->RoundLotForValueCalc * price * m_instrDet->PriceFactor;
        }
    }

    if (noError && m_pThresholds->m_checkForExposure)
    {
        if (currentValue < 0)
        {
            currentValue = currentValue * -1;
        }
        if (currentValue > m_pThresholds->singleTickerNetValueLimit)
        {
            ILOG(FATAL) << "TICKER_LIMIT_CHECK failed" << std::endl;
            ILOG(FATAL) << "Error code = E_RMS_TICKER_VALUE_LIMIT_BREACHED(" << E_RMS_TICKER_VALUE_LIMIT_BREACHED << ")" << std::endl;
            ILOG(FATAL) << "Reason: Current exposure value = " << currentValue << " > singleTickerNetValueLimit = " << m_pThresholds->singleTickerNetValueLimit << std::endl;
            errorNum = E_RMS_TICKER_VALUE_LIMIT_BREACHED;
            noError = false;
        }
        else
        {
            errorNum = E_RMS_NO_ERROR;
        }
    }
    else
    {
        ; //
    }

    tickPos.unlock();
    return noError;
}
/*!
 * OrderFrequencyCheckEnabled is added to let the implementation know if it has to collect the order frequency stats.
 * The aim is improved performance by not doing unnecessary tasks.
 */
bool RmsChecks::OrderFrequencyCheckEnabled() const
{
    return m_checkForOrderFrequency;
}
bool RmsChecks::IsCancelConfirmsCountCheckEnabled() const
{
    return m_cancelConfirmsCountCheckEnabled;
}
/*!
 * GetOrderFrequencyWindow
 */
double RmsChecks::GetOrderFrequencyWindow() const
{
    return m_orderFrequencyWindow;
}
/*!
 * GetOrderPerSecondLimit
 */
size_t RmsChecks::GetOrderPerSecondLimit() const
{
    return m_orderFrequencyLimit;
}

OrderFreqModeCode RmsChecks::GetOrderPerSecondCalcMode() const
{
    return m_orderFreqCalcMode;
}
/*!
 * OrderFrequencyThresholdChecks
 */
bool RmsChecks::OrderFrequencyThresholdChecks(const RequestMsg &req,
                                              OrderPositions &positions, int32_t &errorNum)
{
    bool noError = true;
    double time_start = positions.ordersPerSecond->getStartTime();
    double timeDiff = (positions.orderTime * 1.0 - time_start);

    if (timeDiff <= m_orderFrequencyWindow)
    {
        // if the current order's time is within the order frequency window then check if the max orders limit has reached
        if (positions.ordersPerSecond->m_orderQueue.isFull())
        {
            errorNum = E_RMS_ORDER_FREQUENCY_LIMIT_BREACHED;
            noError = false;
        }
    }
    return noError;
}
/*!
 * OrdersPerDayThresholdChecks implemented for Russia Currency Markets
 */
bool RmsChecks::OrdersPerDayThresholdChecks(const RequestMsg &req,
                                            OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = positions.tickerIterator->second->thresholds;
    bool noError = true;
    double currentTradedValue = positions.accountIterator->second->ordersPerDay.getTotalTradedValue() * m_instrDet->RoundLotForValueCalc * m_instrDet->PriceFactor;

    int64_t currentAllowedOrdersLimit = (int64_t)(m_pThresholds->m_ordersPerDayFixedLimit + (currentTradedValue * m_pThresholds->m_tradedValueMultiplyFactor) * m_pThresholds->m_orderToCommissionFactor);

    int64_t totalOrdersSoFar = positions.accountIterator->second->ordersPerDay.getNewOrdersCount() + 1;
    double percent = (double)totalOrdersSoFar / (double)currentAllowedOrdersLimit;

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << m_pThresholds->m_ordersPerDayFixedLimit << "|" << currentAllowedOrdersLimit << "|" << totalOrdersSoFar << "|" << percent << "\n";
#endif

    if (percent >= m_pThresholds->m_orderPerDayLowerLimit && percent < m_pThresholds->m_orderPerDayUpperLimit)
    {
        errorNum = E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_WARNING;
    }
    else if (percent >= m_pThresholds->m_orderPerDayUpperLimit)
    {
        noError = false;
        errorNum = E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_BREACHED;
    }

    return noError;
}
/*!
 * OrdersPerTradeThresholdChecks - this implementation is more standard
 */
bool RmsChecks::OrdersPerTradeThresholdChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = positions.tickerIterator->second->thresholds;
    bool noError = true;
    int64_t totalOrdersSentSoFar = positions.accountIterator->second->ordersPerDay.getNewOrdersCount() + 1;
    int64_t allowedOrdersPerTrade = positions.accountIterator->second->ordersPerDay.getTotalTradeCount() * m_pThresholds->m_orderPerTradeLimit;
    // fix:第一笔报单时，成交笔数为0
    double percent = allowedOrdersPerTrade == 0 ? 1 : (double)totalOrdersSentSoFar / (double)allowedOrdersPerTrade;

    if (percent >= m_pThresholds->m_orderPerTradeLowerLimit && percent < m_pThresholds->m_orderPerTradeUpperLimit)
    {
        errorNum = E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_WARNING;
    }
    else if (percent >= m_pThresholds->m_orderPerTradeUpperLimit)
    {
        noError = false;
        errorNum = E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_BREACHED;
    }

    return noError;
}

/*!
 * OrdersPerTradeThresholdChecks - this implementation is more standard
 */
bool RmsChecks::KRXTotalOrdersPerTradeUsingTradedQtyChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = positions.tickerIterator->second->thresholds;
    uint64_t ordersPerTradeRatio = 20;

    bool noError = true;
    int64_t totalOrdersSentSoFar = positions.accountIterator->second->ordersPerDay.getNewOrdersCount() +
                                   positions.accountIterator->second->ordersPerDay.getCxlOrdersCount() + 1;

    if (totalOrdersSentSoFar < m_pThresholds->m_ordersPerDayFixedLimit)
    {
        return true;
    }
    else if (totalOrdersSentSoFar >= 99000)
    {
        ordersPerTradeRatio = 10;
    }

    int64_t allowedOrdersPerTrade = positions.accountIterator->second->getTotalTradedQty() * m_pThresholds->m_orderPerTradeLimit;

    double percent = (allowedOrdersPerTrade != 0) ? ((double)totalOrdersSentSoFar / (double)allowedOrdersPerTrade) : 1;

    if (percent >= m_pThresholds->m_orderPerTradeLowerLimit && percent < m_pThresholds->m_orderPerTradeUpperLimit)
    {
        ILOG(WARN) << "TotalOrders: " << totalOrdersSentSoFar
                   << " AllowedOrders: " << allowedOrdersPerTrade
                   << " Ratio: " << setprecision(2) << percent << "\n";
        errorNum = E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_WARNING;
    }
    else if (percent >= m_pThresholds->m_orderPerTradeUpperLimit)
    {
        ILOG(FATAL) << "TotalOrders: " << totalOrdersSentSoFar
                    << " AllowedOrders: " << allowedOrdersPerTrade
                    << " Ratio: " << setprecision(2) << percent << "\n";
        noError = false;
        errorNum = E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_BREACHED;
    }

    return noError;
}

double RmsChecks::GetNetLossPerTicker(const TickerPositions &tickerPosition, double lastTrdPx)
{
    double unrealizedBuyTradedValue = 0.0, unrealizedSellTradedValue = 0.0;

    // Calculate realized PNL
    double buyTradedQty = tickerPosition.netBuyTradedQty * tickerPosition.m_instrumentDetails.RoundLotForValueCalc;
    double sellTradedQty = tickerPosition.netSellTradedQty * tickerPosition.m_instrumentDetails.RoundLotForValueCalc;

    double realizedBuyTradedValue = tickerPosition.netBuyTradedValue * tickerPosition.m_instrumentDetails.PriceFactor * tickerPosition.m_instrumentDetails.RoundLotForValueCalc;
    double realizedSellTradedValue = tickerPosition.netSellTradedValue * tickerPosition.m_instrumentDetails.PriceFactor * tickerPosition.m_instrumentDetails.RoundLotForValueCalc;

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "pricefactor=" << tickerPosition.m_instrumentDetails.PriceFactor << "|roundlog=" << tickerPosition.m_instrumentDetails.RoundLotForValueCalc << "\n";
    ILOG(INFO) << "realizedBuyTradedValue = " << realizedBuyTradedValue << ", realizedSellTradedValue = " << realizedSellTradedValue << "\n";
#endif

    double totalTxnCost = 0.0;
    switch (tickerPosition.thresholds->m_transactionCostCalculator)
    {
    case TXN_COST_ON_TRADED_QTY:
    {
        totalTxnCost = TxnCostCalculator::txnCostOnTradedQty(tickerPosition);
        break;
    }
    case TXN_COST_ON_TRADED_VALUE:
    {
        totalTxnCost = TxnCostCalculator::txnCostOnTradedValue(tickerPosition);
        break;
    }
    case TXN_COST_ON_TRADED_QTY_AND_VALUE:
    {
        totalTxnCost = TxnCostCalculator::txnCostOnTradedQtyAndValue(tickerPosition);
        break;
    }
    default:
        break;
    }

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "totalTxnCost = " << totalTxnCost << "\n";
#endif

    if (buyTradedQty > sellTradedQty) // the account is long
    {
        double unrealizedBuyTradedQty = buyTradedQty - sellTradedQty;
        unrealizedSellTradedValue = (unrealizedBuyTradedQty * lastTrdPx * tickerPosition.m_instrumentDetails.PriceFactor);
        unrealizedBuyTradedValue = 0;
    }
    else if (sellTradedQty > buyTradedQty) // the account is short
    {
        double unrealizedSellTradedQty = sellTradedQty - buyTradedQty;
        unrealizedBuyTradedValue += (unrealizedSellTradedQty * lastTrdPx * tickerPosition.m_instrumentDetails.PriceFactor);
        unrealizedSellTradedValue = 0;
    }
    else // the ticker is net
    {
        // nothing to do
    }

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "unrealizedBuyTradedValue = " << unrealizedBuyTradedValue << ", unrealizedSellTradedValue = " << unrealizedSellTradedValue << "\n";
#endif

    double totalLoss = (realizedBuyTradedValue + unrealizedBuyTradedValue) + totalTxnCost - (realizedSellTradedValue + unrealizedSellTradedValue);

    return totalLoss;
}

/*!
 * StopLossTickerLevelCheck with tickerLevelLoss
 */
bool illuminati::infra::RmsChecks::StopLossPerTickerCheck(double &tickerLoss, const RMSLimits *thresholds, int32_t &errorNum)
{
    bool noError = true;

    if (tickerLoss >= thresholds->m_tickerLevelLossWarningLimit && tickerLoss < thresholds->m_tickerLevelLossFatalLimit)
    {

#if defined(_PRINT_UPDATE)
        ILOG(WARN) << "Loss On Ticker: " << tickerLoss
                   << " WarnLimit: " << thresholds->m_tickerLevelLossWarningLimit
                   << " FatalLimit: " << thresholds->m_tickerLevelLossFatalLimit << "\n";
#endif

        errorNum = E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_WARNING;
    }
    else if (tickerLoss >= thresholds->m_tickerLevelLossFatalLimit)
    {
        noError = false;
        errorNum = E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_BREACHED;
    }

    return noError;
}

/*!
 * StopLossAccountLevelCheck with accountLevelLoss
 */
bool RmsChecks::StopLossAccountLevelCheck(double &accountLevelLoss, const RMSLimits *thresholds, int32_t &errorNum)
{
    bool noError = true;
    if (accountLevelLoss >= thresholds->m_accountLevelLossWarningLimit && accountLevelLoss < thresholds->m_accountLevelLossFatalLimit)
    {
#if defined(_PRINT_UPDATE)
        ILOG(WARN) << "Loss On Account: " << accountLevelLoss
                   << " WarnLimit: " << thresholds->m_accountLevelLossWarningLimit
                   << " FatalLimit: " << thresholds->m_accountLevelLossFatalLimit << "\n";
#endif

        errorNum = E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING;
    }
    else if (accountLevelLoss >= thresholds->m_accountLevelLossFatalLimit)
    {
        noError = false;
        errorNum = E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_BREACHED;
    }

    return noError;
}
/*!
 * StopLossAccountLevelCheck
 */
bool RmsChecks::StopLossAccountLevelCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum)
{
    bool noError = true;
    m_pThresholds = positions.tickerIterator->second->thresholds;
    double latestTradePriceFromMdUpdates = 0.0;
    double tickerLevelLoss = 0.0;
    double accountLevelLoss = 0.0;

    TickerLTPUpdates &ltpUpdate = *(positions.tickerIterator->second->tickLtpIterator->second);
    latestTradePriceFromMdUpdates = ltpUpdate.get();

    AccountPositions &accountPos = *(positions.accountIterator->second);
    for (auto iter = accountPos.perTickerPositions.begin();
         iter != accountPos.perTickerPositions.end(); iter++)
    {
        TickerPositions &tickerPosition = *(iter->second);
        tickerPosition.lock();
        tickerLevelLoss = GetNetLossPerTicker(tickerPosition, latestTradePriceFromMdUpdates);
        tickerPosition.unlock();

        accountLevelLoss += tickerLevelLoss;
    }

    if (accountLevelLoss >= m_pThresholds->m_accountLevelLossWarningLimit && accountLevelLoss < m_pThresholds->m_accountLevelLossFatalLimit)
    {
        errorNum = E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING;
    }
    else if (accountLevelLoss >= m_pThresholds->m_accountLevelLossFatalLimit)
    {
        noError = false;
        errorNum = E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_BREACHED;
    }

    return noError;
}

bool RmsChecks::StopLossCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum)
{
    m_firmLevelThresholds = m_rmsConfig.getFirmwideLimits();
    m_pThresholds = positions.tickerIterator->second->thresholds;

    bool noError = true;
    double latestTradePriceFromMdUpdates = 0.0;
    double tickerLevelLoss = 0.0;
    double accountLevelLoss = 0.0;
    double firmLevelLoss = 0.0;

    string currentOrderSymbol = positions.tickerIterator->second->symbol;
    string currentOrderAccount = positions.accountIterator->second->accountId;

    for (auto iter = positions.allAccountsPointer->begin(); iter != positions.allAccountsPointer->end(); iter++)
    {
        AccountPositions &accountPos = *(iter->second);
        for (auto iter = accountPos.perTickerPositions.begin(); iter != accountPos.perTickerPositions.end(); iter++)
        {
            TickerPositions &tickerPosition = *(iter->second);
            tickerPosition.lock();
            TickerLTPUpdates &ltpUpdate = *(tickerPosition.tickLtpIterator->second);
            latestTradePriceFromMdUpdates = ltpUpdate.get();

            tickerLevelLoss = GetNetLossPerTicker(tickerPosition, latestTradePriceFromMdUpdates);
            noError = StopLossPerTickerCheck(tickerLevelLoss, m_pThresholds, errorNum);
            tickerPosition.unlock();

#if defined(_PRINT_UPDATE)
            ILOG(INFO) << tickerPosition.symbol << " ticker level loss = " << tickerLevelLoss
                       << ", ticker warning threshold = " << m_pThresholds->m_tickerLevelLossWarningLimit
                       << ", ticker fatal threshold = " << m_pThresholds->m_tickerLevelLossFatalLimit << "\n";
#endif

            if (!noError && currentOrderSymbol == tickerPosition.symbol)
            {
                ILOG(FATAL) << tickerPosition.symbol << " ticker level loss = " << tickerLevelLoss
                            << ", ticker warning threshold = " << m_pThresholds->m_tickerLevelLossWarningLimit
                            << ", ticker fatal threshold = " << m_pThresholds->m_tickerLevelLossFatalLimit << "\n";
                return noError;
            }

            accountLevelLoss += tickerLevelLoss;
        }

#if defined(_PRINT_UPDATE)
        ILOG(INFO) << accountPos.accountId << " account level loss = " << accountLevelLoss
                   << ", account warning threshold = " << m_pThresholds->m_accountLevelLossWarningLimit
                   << ", account fatal threshold = " << m_pThresholds->m_accountLevelLossFatalLimit << "\n";
#endif

        noError = StopLossAccountLevelCheck(accountLevelLoss, m_pThresholds, errorNum);
        if (!noError && currentOrderAccount == accountPos.accountId)
        {
            ILOG(FATAL) << accountPos.accountId << " account level loss = " << accountLevelLoss
                        << ", account warning threshold = " << m_pThresholds->m_accountLevelLossWarningLimit
                        << ", account fatal threshold = " << m_pThresholds->m_accountLevelLossFatalLimit << "\n";
            return noError;
        }

        firmLevelLoss += accountLevelLoss;
    }

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "firm level loss = " << firmLevelLoss << ", limit is = " << m_firmLevelThresholds->m_firmLevelLossFatalLimit << "\n";
#endif

    if (firmLevelLoss >= m_firmLevelThresholds->m_firmLevelLossFatalLimit)
    {
        ILOG(FATAL) << "firm level loss = " << firmLevelLoss << ", limit is = " << m_firmLevelThresholds->m_firmLevelLossFatalLimit << "\n";
        noError = false;
        errorNum = E_RMS_FIRM_LEVEL_STOP_LOSS_LIMIT_BREACHED;
    }

    return noError;
}

bool RmsChecks::NetPositionFirmLevelCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = m_rmsConfig.getFirmwideLimits();
    double netBuyQty = 0;
    double netSellQty = 0;
    for (auto index = positions.allAccountsPointer->begin();
         index != positions.allAccountsPointer->end(); index++)
    {
        auto instrIndex = index->second->perTickerPositions.find(req.Contract_Description.Symbol);
        if (instrIndex != index->second->perTickerPositions.end())
        {
            if (instrIndex->second->symbol == req.Contract_Description.Symbol)
            {
                instrIndex->second->lock();
                netBuyQty += instrIndex->second->netBuyTradedQty;
                netSellQty += instrIndex->second->netSellTradedQty;
                instrIndex->second->unlock();

                if (RequestType::NEWORDER == req.Request_Type)
                {
                    if (illuminati::md::SIDE_BUY == req.Transaction_Type)
                    {
                        netBuyQty += req.Quantity;
                    }
                    else
                    {
                        netSellQty += req.Quantity;
                    }
                }
                else if (RequestType::MODIFYORDER == req.Request_Type)
                {
                    if (illuminati::md::SIDE_BUY == req.Transaction_Type)
                    {
                        // Just check for new quantity. Don't need to subtract the old one
                        netBuyQty += req.Quantity;
                    }
                    else
                    {
                        // Just check for new quantity. Don't need to subtract the old one
                        netSellQty += req.Quantity;
                    }
                }
                break;
            }
        }
    }
    double netPosition = netSellQty - netBuyQty;

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "NetPosition in lots = " << netPosition << "\n";
#endif

    if (fabs(netPosition) > m_pThresholds->m_firmwideNetPositionInLotsLimit)
    {
        ILOG(FATAL) << "QUANTITY_FIRMLEVEL_CHECK failed" << std::endl;
        ILOG(FATAL) << "Error code = E_RMS_NET_POSITION_IN_LOTS_FIRMLEVEL(" << E_RMS_NET_POSITION_IN_LOTS_FIRMLEVEL << ")" << std::endl;
        ILOG(FATAL) << "Reason: netPosition = " << fabs(netPosition) << " >= firmwideNetPositionInLotsLimit = " << m_pThresholds->m_firmwideNetPositionInLotsLimit << std::endl;
        errorNum = E_RMS_NET_POSITION_IN_LOTS_FIRMLEVEL;
        return false;
    }

    // Check for max qty for a single order in the firm level
    if (req.Quantity > m_pThresholds->m_firmwideOpenQtyLimit)
    {
        ILOG(FATAL) << "QUANTITY_FIRMLEVEL_CHECK failed" << std::endl;
        ILOG(FATAL) << "Error code = E_RMS_QUANTITY_PERORDER_IN_LOTS_FIRMLEVEL(" << E_RMS_QUANTITY_PERORDER_IN_LOTS_FIRMLEVEL << ")" << std::endl;
        ILOG(FATAL) << "Reason: order quantity = " << req.Quantity << " >= firmwideOpenQtyLimit  = " << m_pThresholds->m_firmwideOpenQtyLimit << std::endl;
        errorNum = E_RMS_QUANTITY_PERORDER_IN_LOTS_FIRMLEVEL;
        return false;
    }
    return true;
}

bool RmsChecks::QuantityFirmLevelCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = m_rmsConfig.getFirmwideLimits();
    double netBuyQty = 0;
    double netSellQty = 0;
    for (auto index = positions.allAccountsPointer->begin();
         index != positions.allAccountsPointer->end(); index++)
    {
        auto instrIndex = index->second->perTickerPositions.find(req.Contract_Description.Symbol);
        if (instrIndex != index->second->perTickerPositions.end())
        {
            if (instrIndex->second->symbol == req.Contract_Description.Symbol)
            {
                instrIndex->second->lock();
                netBuyQty += instrIndex->second->netBuyTradedQty;
                // This contains the prospective open quantities
                netBuyQty += instrIndex->second->netBuyOpenQty + instrIndex->second->netBuyModifySentButNotConfirmedQty;
                netSellQty += instrIndex->second->netSellTradedQty;
                // This contains the prospective open quantities
                netSellQty += instrIndex->second->netSellOpenQty + instrIndex->second->netSellModifySentButNotConfirmedQty;
                instrIndex->second->unlock();

                if (RequestType::NEWORDER == req.Request_Type)
                {
                    if (illuminati::md::SIDE_BUY == req.Transaction_Type)
                    {
                        netBuyQty += req.Quantity;
                    }
                    else
                    {
                        netSellQty += req.Quantity;
                    }
                }
                else if (RequestType::MODIFYORDER == req.Request_Type)
                {
                    if (illuminati::md::SIDE_BUY == req.Transaction_Type)
                    {
                        netBuyQty += req.Quantity;
                    }
                    else
                    {
                        netSellQty += req.Quantity;
                    }
                }
                break;
            }
        }
    }
    double netPosition = netSellQty - netBuyQty;

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "NetPosition in lots = " << netPosition << "\n";
#endif

    if (fabs(netPosition) > m_pThresholds->m_firmwideNetPositionInLotsLimit)
    {
        errorNum = E_RMS_TOTAL_POSITION_IN_LOTS_FIRMLEVEL;
        return false;
    }

    // Check for max qty for a single order in the firm level
    if (req.Quantity > m_pThresholds->m_firmwideOpenQtyLimit)
    {
        errorNum = E_RMS_QUANTITY_PERORDER_IN_LOTS_FIRMLEVEL;
        return false;
    }
    return true;
}

bool RmsChecks::XPercentAwayFromLTPCheck(const RequestMsg &req,
                                         OrderPositions &positions, int32_t &errorNum)
{
    m_pThresholds = positions.tickerIterator->second->thresholds;

    double ltp;
    TickerLTPUpdates &ltpUpdate =
        *(positions.tickerIterator->second->tickLtpIterator->second);
    ltpUpdate.lock();
    ltp = ltpUpdate.lastTradePrice + ltpUpdate.ltpAlternative;
    ltpUpdate.unlock();

#if defined(_PRINT_UPDATE)
    ILOG(INFO) << "LTP is " << ltp << ", Current Price is " << req.Price << "\n";
#endif

    auto x = m_pThresholds->m_percentageAwayFromLTPLimit * ltp;

    if (req.Price < (ltp - x) || req.Price > (x + ltp))
    {
        errorNum = E_RMS_TICKER_LEVEL_PERCENTAGE_AWAY_FROM_X_FAILED;
        return false;
    }

    return true;
}

bool RmsChecks::CancelConfirmationCountCheck(const RequestMsg &req,
                                             OrderPositions &positions, int32_t &errorNum)
{
    bool noError = true;
    m_pThresholds = positions.tickerIterator->second->thresholds;

    if (req.OrdType == LIMIT && req.Duration == DAY)
    {
        if (positions.tickerIterator->second->totalCancelOrderConfirms + 1 > m_pThresholds->m_maxCancelConfirmationLimit)
        {
            errorNum = E_RMS_MAX_CANCEL_CONFIRMS_LIMIT_BREACHED;
            noError = false;
        }
    }

    return noError;
}

bool RmsChecks::NegativePriceOrQuantityCheck(const RequestMsg &req,
                                             OrderPositions &positions, int32_t &errorNum)
{
    bool noError = true;

    if (req.Price < 0)
    {
        errorNum = E_RMS_FOUND_NEGATIVE_PRICE;
        noError = false;
    }

    if (req.Quantity < 0)
    {
        errorNum = E_RMS_FOUND_NEGATIVE_QUANTITY;
        noError = false;
    }

    return noError;
}

#ifndef _INFRA_RMSLIMITS_H
#define _INFRA_RMSLIMITS_H

#include "xmlreader.h"
#include "logger.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdio>
#include "xorcipher.h"

enum OrderFreqModeCode
{
    ROLLING,
    DISCRETE
};

struct RMSLimits
{
    RMSLimits() : singleOrderOpenQtyLimit(0.0), singleOrderOpenValueLimit(0.0), singleTickerOpenOrdersLimit(
                                                                                    0.0),
                  singleTickerNetValueLimit(0.0), m_checkForExposure(
                                                      false),
                  m_ordersPerDayFixedLimit(0), m_tradedValueMultiplyFactor(
                                                   0.0),
                  m_orderToCommissionFactor(0.0), m_orderPerDayLowerLimit(
                                                      0.0),
                  m_orderPerDayUpperLimit(0.0), m_orderPerTradeLimit(
                                                    0),
                  m_orderPerTradeLowerLimit(0.0), m_orderPerTradeUpperLimit(
                                                      0.0),
                  m_singleTickerNetPositionInLotsLimit(0.0), m_firmwideNetPositionInLotsLimit(
                                                                 0.0),
                  m_firmwideOpenQtyLimit(0.0), m_orderFrequencyWindow(
                                                   0.0),
                  m_orderFrequencyLimit(0), m_transactionCostCalculator(
                                                0.0),
                  m_buyTransactionCostFactorOnQty(0.0), m_buyTransactionCostFactorOnValue(
                                                            0.0),
                  m_sellTransactionCostFactorOnQty(0.0), m_sellTransactionCostFactorOnValue(
                                                             0.0),
                  m_accountLevelLossWarningLimit(0.0), m_accountLevelLossFatalLimit(
                                                           0.0),
                  m_firmLevelLossFatalLimit(0.0), m_tickerLevelLossWarningLimit(
                                                      0.0),
                  m_tickerLevelLossFatalLimit(0.0), m_percentageAwayFromLTPLimit(0.0),
                  m_maxCancelConfirmationLimit(0), m_orderFreqCalcMode(ROLLING)
    {
    }

    void reset()
    {
        singleOrderOpenQtyLimit = 0.0;
        singleOrderOpenValueLimit = 0.0;
        singleTickerOpenOrdersLimit = 0.0;
        singleTickerNetValueLimit = 0.0;
        m_checkForExposure = false;
        m_ordersPerDayFixedLimit = 0;
        m_tradedValueMultiplyFactor = 0.0;
        m_orderToCommissionFactor = 0.0;
        m_orderPerDayLowerLimit = 0.0;
        m_orderPerDayUpperLimit = 0.0;
        m_orderPerTradeLimit = 0;
        m_orderPerTradeLowerLimit = 0.0;
        m_orderPerTradeUpperLimit = 0.0;
        m_singleTickerNetPositionInLotsLimit = 0.0;
        m_firmwideNetPositionInLotsLimit = 0.0;
        m_firmwideOpenQtyLimit = 0.0;
        m_orderFrequencyWindow = 0.0;
        m_orderFrequencyLimit = 0;
        m_transactionCostCalculator = 0.0;
        m_buyTransactionCostFactorOnQty = 0.0;
        m_buyTransactionCostFactorOnValue = 0.0;
        m_sellTransactionCostFactorOnQty = 0.0;
        m_sellTransactionCostFactorOnValue = 0.0;
        m_accountLevelLossWarningLimit = 0.0;
        m_accountLevelLossFatalLimit = 0.0;
        m_firmLevelLossFatalLimit = 0.0;
        m_tickerLevelLossWarningLimit = 0.0;
        m_tickerLevelLossFatalLimit = 0.0;
        m_percentageAwayFromLTPLimit = 0.0;
        m_maxCancelConfirmationLimit = 0.0;
        m_orderFreqCalcMode = ROLLING;
    }

    double singleOrderOpenQtyLimit;
    double singleOrderOpenValueLimit;
    double singleTickerOpenOrdersLimit;
    double singleTickerNetValueLimit;
    bool m_checkForExposure;
    int64_t m_ordersPerDayFixedLimit;
    double m_tradedValueMultiplyFactor;
    double m_orderToCommissionFactor;
    double m_orderPerDayLowerLimit;
    double m_orderPerDayUpperLimit;
    int64_t m_orderPerTradeLimit;
    double m_orderPerTradeLowerLimit;
    double m_orderPerTradeUpperLimit;
    double m_singleTickerNetPositionInLotsLimit;
    double m_firmwideNetPositionInLotsLimit;
    double m_firmwideOpenQtyLimit;
    double m_orderFrequencyWindow;
    uint32_t m_orderFrequencyLimit;
    uint32_t m_transactionCostCalculator;
    double m_buyTransactionCostFactorOnQty;
    double m_buyTransactionCostFactorOnValue;
    double m_sellTransactionCostFactorOnQty;
    double m_sellTransactionCostFactorOnValue;
    double m_accountLevelLossWarningLimit;
    double m_accountLevelLossFatalLimit;
    double m_firmLevelLossFatalLimit;
    double m_tickerLevelLossWarningLimit;
    double m_tickerLevelLossFatalLimit;
    double m_percentageAwayFromLTPLimit;
    uint32_t m_maxCancelConfirmationLimit;
    OrderFreqModeCode m_orderFreqCalcMode;

    void printFirmwideLimits()
    {
        ILOG(INFO) << "FirmwideOrderFrequencyWindow:" << m_orderFrequencyWindow << "|"
                   << "FirmwideOrderFrequencyLimit:" << m_orderFrequencyLimit << "|"
                   << "FirmwidePerTickerNetPositionInLots:" << m_firmwideNetPositionInLotsLimit << "|"
                   << "FirmwideSingleOrderQtyLimit:" << m_firmwideOpenQtyLimit << "|"
                   << "FirmLevelLossFatalLimit:" << m_firmLevelLossFatalLimit << "|"
                   << "FirmLevelOrderFrequenceMode:" << m_orderFreqCalcMode << "|\n";
        //                    <<"OrderToCommissionFactor:" << m_orderToCommissionFactor<<"|"
        //                    <<"TradedValueMultiplyFactor:"<<m_tradedValueMultiplyFactor<<"\n";
    }

    void printGeneralLimits()
    {
        ILOG(INFO) << "MaxOpenOrderPerTicker:" << singleTickerOpenOrdersLimit << "|"
                   << "PercentageAwayFromLTPLimit:" << m_percentageAwayFromLTPLimit << "|"
                   << "PerTickerNetValueLimit:" << singleTickerNetValueLimit << "|"
                   << "PerTickerNetPositionInLotsLimit:" << m_singleTickerNetPositionInLotsLimit << "|"
                   << "PerOrderOpenValueLimit:" << singleOrderOpenValueLimit << "|"
                   << "PerOrderOpenQtyLimit:" << singleOrderOpenQtyLimit << "|"
                   << "CheckForExposure:" << m_checkForExposure << "|"
                   << "OrdersPerDayFixedLimit:" << m_ordersPerDayFixedLimit << "|"
                   << "orderPerDayLowerLimit:" << m_orderPerDayLowerLimit << "|"
                   << "orderPerDayUpperLimit:" << m_orderPerDayUpperLimit << "|"
                   << "transactionCostCalculator:" << m_transactionCostCalculator << "|"
                   << "buyTransactionCostFactorOnQty:" << m_buyTransactionCostFactorOnQty << "|"
                   << "buyTransactionCostFactorOnValue:" << m_buyTransactionCostFactorOnValue << "|"
                   << "sellTransactionCostFactorOnQty:" << m_sellTransactionCostFactorOnQty << "|"
                   << "sellTransactionCostFactorOnValue:" << m_sellTransactionCostFactorOnValue << "|"
                   << "accountLevelLossWarningLimit:" << m_accountLevelLossWarningLimit << "|"
                   << "accountLevelLossFatalLimit:" << m_accountLevelLossFatalLimit << "|"
                   << "tickerLevelLossWarningLimit:" << m_tickerLevelLossWarningLimit << "|"
                   << "tickerLevelLossFatalLimit:" << m_tickerLevelLossFatalLimit << "|"
                   << "maxCancelConfirmationLimit:" << m_maxCancelConfirmationLimit << "\n";
    }

    void printLimits()
    {
        printFirmwideLimits();
        printGeneralLimits();
    }
};

struct RmsAccountLimits
{
public:
    RMSLimits m_defaultLimits;
    std::unordered_map<std::string, RMSLimits> m_instrLimits;

    const RMSLimits *getRmsLimits(std::string instr)
    {
        auto iter = m_instrLimits.find(instr);
        if (iter == m_instrLimits.end())
            return const_cast<RMSLimits *>(&m_defaultLimits);
        return const_cast<RMSLimits *>(&(iter->second));
    }

    RMSLimits *getInstrumentLimits(std::string instrName)
    {
        auto idx = m_instrLimits.find(instrName);
        if (idx != m_instrLimits.end())
            return &(idx->second);
        return &m_defaultLimits;
    }

    void printLimits()
    {
        ILOG(INFO) << "Default limits:\n";
        m_defaultLimits.printGeneralLimits();
        ILOG(INFO) << "Instruments limits: \n";
        for (auto idx = m_instrLimits.begin(); idx != m_instrLimits.end(); idx++)
        {
            ILOG(INFO) << "Instrument name: " << idx->first << "\n";
            ILOG(INFO) << "Limits for the instrument: \n";
            idx->second.printGeneralLimits();
        }
    }
};

class RmsConfig
{
private:
    RMSLimits m_firmwideLimits;
    std::unordered_map<std::string, RmsAccountLimits> m_accountwiseLimits;
    // We have to iterate through to the checks to setup our rules. So vector is better
    std::vector<std::string> m_enabledChecks;
    // Only used in searching for symbols, so set is better
    std::unordered_set<std::string> m_enabledSymbols;
    std::unordered_set<std::string> m_bannedSymbols;
    std::string firmwideConfigKey; // Key used to decrypt the firmwide config
public:
    void setFirmwideConfigKey(std::string key)
    {
        firmwideConfigKey = key;
    }

    void reset()
    {
        m_accountwiseLimits.clear();
        m_enabledChecks.clear();
        m_enabledSymbols.clear();
        m_bannedSymbols.clear();
    }
    std::vector<std::string> getAllAccounts()
    {
        std::vector<std::string> accounts;
        for (auto idx = m_accountwiseLimits.begin();
             idx != m_accountwiseLimits.end(); idx++)
        {
            accounts.push_back(idx->first);
        }
        return accounts;
    }

    std::vector<std::string> getEnabledChecks()
    {
        return m_enabledChecks;
    }

    std::unordered_set<std::string> getEnabledSymbols()
    {
        return m_enabledSymbols;
    }

    std::unordered_set<std::string> getBannedSymbols()
    {
        return m_bannedSymbols;
    }

    RmsAccountLimits *getAccountLimits(std::string account)
    {
        auto idx = m_accountwiseLimits.find(account);
        if (idx != m_accountwiseLimits.end())
            return &(idx->second);
        return NULL;
    }

    RMSLimits *getFirmwideLimits()
    {
        return &m_firmwideLimits;
    }

    void printFirmwideLimits()
    {
        ILOG(INFO) << "Firmwide limits:\n";
        m_firmwideLimits.printFirmwideLimits();
    }

    void printAccountwiseLimits()
    {
        ILOG(INFO) << "Accountwise limits:\n";
        for (auto acctidx = m_accountwiseLimits.begin(); acctidx != m_accountwiseLimits.end(); acctidx++)
        {
            ILOG(INFO) << "Account: " << acctidx->first << "\n";
            auto acctlimits = acctidx->second;
            acctlimits.printLimits();
        }
    }

    void print()
    {
        ILOG(INFO) << "Enabled checks: \n";
        for (size_t i = 0; i < m_enabledChecks.size(); i++)
        {
            ILOG(INFO) << m_enabledChecks[i] << "\n";
        }
        ILOG(INFO) << "Enabled symbols:\n";
        for (auto index = m_enabledSymbols.begin(); index != m_enabledSymbols.end(); index++)
        {
            ILOG(INFO) << (*index) << "\n";
        }
        ILOG(INFO) << "Banned symbols:\n";
        for (auto index = m_bannedSymbols.begin(); index != m_bannedSymbols.end(); index++)
        {
            ILOG(INFO) << (*index) << "\n";
        }
        printFirmwideLimits();
        printAccountwiseLimits();
    }

    void loadFirmwideLimits(std::string firmwideLimitsConfig)
    {
        ILOG(INFO) << "Loading firmwide config from: " << firmwideLimitsConfig << "\n";
        illuminati::XmlReader reader;
        if (firmwideConfigKey.size() > 0)
        {
            ILOG(INFO) << "Firmwide config key is present: " << firmwideConfigKey << ": decrypting the config\n";
            // read full contents
            FILE *fp = fopen(firmwideLimitsConfig.c_str(), "rb");
            fseek(fp, 0, SEEK_END);
            size_t size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (size == 0)
            {
                ILOG(FATAL) << "Firmwide configuration is empty: " << firmwideLimitsConfig << "\n";
                throw "Firmwide configuration is empty";
            }

            char *buffer = new char[size + 1];
            size_t read = fread(buffer, 1, size, fp);
            if (read != size)
            {
                ILOG(FATAL) << "Error in reading firmwide configuration: " << firmwideLimitsConfig << "\n";
                throw("Error in reading firmwide configuration");
            }
            char *outputbuffer = new char[size + 1];
            illuminati::security::XORCipher cipher(firmwideConfigKey);
            cipher.decrypt(buffer, size, outputbuffer);
            outputbuffer[size] = 0;
            ILOG(INFO) << "After decrypting: \n";
            ILOG(INFO) << outputbuffer << "\n";

            reader.parse(outputbuffer, size);
            delete[] buffer;
            delete[] outputbuffer;
        }
        else
        {
            ILOG(INFO) << "Firmwide config key is not present\n";
            if (!reader.LoadConfig(firmwideLimitsConfig))
            {
                ILOG(FATAL) << "Unable to load config: " << firmwideLimitsConfig << "\n";
                throw std::string("Unable to load config");
            }
        }

        if (m_enabledSymbols.size())
            m_enabledSymbols.clear();

        if (reader.childElement("EnabledSymbols"))
        {
            auto iter = reader.childElement("Symbol");
            if (iter != NULL)
            {
                while (iter)
                {
                    m_enabledSymbols.insert(iter->getString());
                    iter = iter->nextElement("Symbol");
                }
                reader.parentElement();
            }
        }
        else
        {
            ILOG(FATAL) << "Mandatory tag EnabledSymbols missing in xml doc: " << firmwideLimitsConfig << "\n";
            throw std::string("Mandatory tag EnabledSymbols missing in xml doc");
        }

        // Reading Firmwide limits
        reader.nextElement("Firmwide");
        m_firmwideLimits.m_orderFrequencyWindow = reader.getDouble("OrderFrequencyWindow", 1);
        m_firmwideLimits.m_orderFrequencyLimit = reader.getInt("OrderFrequencyLimit");
        std::string orderFreqCalcModeStr = reader.getString("OrderFreqCalcMode");
        if (orderFreqCalcModeStr == "ROLLING")
            m_firmwideLimits.m_orderFreqCalcMode = ROLLING;
        else if (orderFreqCalcModeStr == "DISCRETE")
            m_firmwideLimits.m_orderFreqCalcMode = DISCRETE;
        else
            m_firmwideLimits.m_orderFreqCalcMode = ROLLING;

        m_firmwideLimits.m_firmwideNetPositionInLotsLimit = reader.getDouble("PerTickerNetQuantity");
        if (m_firmwideLimits.m_firmwideNetPositionInLotsLimit == 0)
        {
            ILOG(FATAL) << "Firmwide setting for NetPosition in lots is not configured.\n";
            throw "Firmwide setting for NetPosition in lots is not configured";
        }
        m_firmwideLimits.m_orderToCommissionFactor = reader.getDouble("OrderToCommisionFactor");
        m_firmwideLimits.m_tradedValueMultiplyFactor = reader.getDouble("TradedValueMultiplyFactor");
        m_firmwideLimits.m_firmwideOpenQtyLimit = reader.getDouble("OpenQuantity"); // OrderQuantity
        if (m_firmwideLimits.m_firmwideOpenQtyLimit == 0)
        {
            ILOG(FATAL) << "Firmwide setting for NetOpenQuantity in lots is not configured.\n";
            throw "Firmwide setting for NetOpenQuantity in lots is not configured";
        }
        m_firmwideLimits.m_firmLevelLossFatalLimit = reader.getDouble("FirmLevelLossFatalLimit");
        if (m_firmwideLimits.m_firmLevelLossFatalLimit == 0)
        {
            ILOG(FATAL) << "Firmwide setting for LossFatalLimit is not set.\n";
            throw "Firmwide setting for LossFatalLimit is not set";
        }
    }

    template <typename T>
    void setLimits(illuminati::XmlReader *reader, T &limit, T defaultLimit,
                   std::string tagName, bool bshouldDefault)
    {
        limit = reader->getDouble(tagName);
        if (bshouldDefault && limit == 0)
            limit = defaultLimit;
        else if (limit == 0)
        {
            std::string error = tagName;
            error += " not specified in the limits";
            throw error;
        }
    }

    void loadConfig(std::string configPath, std::string firmwideLimitsConfig)
    {
        if (firmwideLimitsConfig.size() == 0 && configPath.size() == 0)
        {
            ILOG(FATAL) << "Configuration path is not set: " << configPath << "|" << firmwideLimitsConfig << "\n";
            throw "Configuration path is not set";
        }
        // reset();
        if (firmwideLimitsConfig.size() == 0)
        {
            ILOG(INFO) << "Using RMS Config to load Firmwide Limits also" << std::endl;
            loadFirmwideLimits(configPath);
        }
        else
        {
            loadFirmwideLimits(firmwideLimitsConfig);
        }

        ILOG(INFO) << "Loading accountwise config from: " << configPath << "\n";
        illuminati::XmlReader reader;
        if (!reader.LoadConfig(configPath))
        {
            ILOG(FATAL) << "Unable to load config: " << configPath << "\n";
            throw std::string("Unable to load config");
        }

        if (m_enabledChecks.size())
        {
            m_enabledChecks.clear();
        }
        // extract checks from xml doc
        if (reader.childElement("EnabledChecks"))
        {
            auto checksIter = reader.childElement("Check");
            if (checksIter != NULL)
            {
                while (checksIter)
                {
                    m_enabledChecks.push_back(checksIter->getString());
                    checksIter = checksIter->nextElement("Check");
                }
                reader.parentElement();
            }
        }
        else
        {
            ILOG(FATAL) << "EnabledChecks element is missing in XML doc: " << configPath << "\n";
            throw std::string("Mandatory element EnabledChecks is missing in xml doc");
        }
        // clear m_bannedSymbols if it already has values.
        if (m_bannedSymbols.size())
        {
            m_bannedSymbols.clear();
        }
        // extract BannedSymbols from xml doc
        if (reader.nextElement("BannedSymbols"))
        {
            auto iter = reader.childElement("Symbol");
            while (iter)
            {
                m_bannedSymbols.insert(iter->getString());
                iter = iter->nextElement("Symbol");
            }
            reader.parentElement();
        }

        // Read accountwise limits
        reader.nextElement("Limits")->childElement("AccountWise");
        auto accountIter = reader.childElement("Account");
        while (accountIter)
        {
            std::string accountName = reader.getStringAttribute("name");
            auto accountFound = m_accountwiseLimits.find(accountName);
            if (accountFound == m_accountwiseLimits.end())
            {
                RmsAccountLimits alimits;
                auto accountInsert = m_accountwiseLimits.insert(make_pair(accountName, alimits));
                accountFound = accountInsert.first;
            }

            RmsAccountLimits *limits = &(accountFound->second);
            // Reading default limits
            accountIter->childElement("DefaultLimits");
            limits->m_defaultLimits.singleOrderOpenValueLimit = accountIter->getDouble("OpenValue");
            limits->m_defaultLimits.singleOrderOpenQtyLimit = accountIter->getDouble("OpenQuantity");
            limits->m_defaultLimits.singleTickerOpenOrdersLimit = accountIter->getDouble("OpenOrders");
            limits->m_defaultLimits.singleTickerNetValueLimit = accountIter->getDouble("PerTickerNetValue");
            limits->m_defaultLimits.m_checkForExposure = accountIter->getBool("EnableExposureCheck");

            limits->m_defaultLimits.m_ordersPerDayFixedLimit = accountIter->getInt64("OrdersPerDay");
            limits->m_defaultLimits.m_orderPerDayLowerLimit = accountIter->getDouble("OrdersPerDayLowerLimit");
            limits->m_defaultLimits.m_orderPerDayUpperLimit = accountIter->getDouble("OrdersPerDayUpperLimit");

            limits->m_defaultLimits.m_orderPerTradeLimit = accountIter->getInt64("OrdersPerTrade");
            limits->m_defaultLimits.m_orderPerTradeLowerLimit = accountIter->getDouble("OrdersPerTradeLowerLimit");
            limits->m_defaultLimits.m_orderPerTradeUpperLimit = accountIter->getDouble("OrdersPerTradeUpperLimit");

            limits->m_defaultLimits.m_singleTickerNetPositionInLotsLimit = accountIter->getDouble("PerTickerNetQuantity");
            limits->m_defaultLimits.m_transactionCostCalculator = accountIter->getDouble("TransactionCostCalculator");
            limits->m_defaultLimits.m_buyTransactionCostFactorOnQty = accountIter->getDouble("BuyTransactionCostOnQtyFactor");
            limits->m_defaultLimits.m_sellTransactionCostFactorOnQty = accountIter->getDouble("SellTransactionCostOnQtyFactor");
            limits->m_defaultLimits.m_buyTransactionCostFactorOnValue = accountIter->getDouble("BuyTransactionCostOnValueFactor");
            limits->m_defaultLimits.m_sellTransactionCostFactorOnValue = accountIter->getDouble("SellTransactionCostOnValueFactor");
            limits->m_defaultLimits.m_accountLevelLossFatalLimit = accountIter->getDouble("AccountLevelLossFatalLimit");
            limits->m_defaultLimits.m_accountLevelLossWarningLimit = accountIter->getDouble("AccountLevelLossWarningLimit");

            limits->m_defaultLimits.m_tickerLevelLossWarningLimit = accountIter->getDouble("InstrumentLevelLossWarningLimit");
            limits->m_defaultLimits.m_tickerLevelLossFatalLimit = accountIter->getDouble("InstrumentLevelLossFatalLimit");
            limits->m_defaultLimits.m_percentageAwayFromLTPLimit = accountIter->getDouble("PercentageAwayFromLTPLimit");
            limits->m_defaultLimits.m_maxCancelConfirmationLimit = accountIter->getInt("MaxCancelConfirmationLimit");

            // Reading instrument specific limits
            auto instrLimitIter = accountIter->nextElement("InstrumentLimits");
            while (instrLimitIter)
            {
                std::string defaultType = instrLimitIter->getStringAttribute("type");
                bool bshouldDefault = (defaultType == "NoDefaults") ? false : true;
                // Load limits for instruments
                auto currentLimits = instrLimitIter->childElement("Limits");
                RMSLimits rl;

                setLimits(currentLimits, rl.singleOrderOpenValueLimit, limits->m_defaultLimits.singleOrderOpenValueLimit,
                          "OpenValue", bshouldDefault);

                setLimits(currentLimits, rl.singleTickerOpenOrdersLimit, limits->m_defaultLimits.singleTickerOpenOrdersLimit,
                          "OpenOrders", bshouldDefault);

                setLimits(currentLimits, rl.m_ordersPerDayFixedLimit, limits->m_defaultLimits.m_ordersPerDayFixedLimit,
                          "OrdersPerDay", bshouldDefault);

                setLimits(currentLimits, rl.m_orderPerDayLowerLimit, limits->m_defaultLimits.m_orderPerDayLowerLimit,
                          "OrdersPerDayLowerLimit", bshouldDefault);

                setLimits(currentLimits, rl.m_orderPerDayUpperLimit, limits->m_defaultLimits.m_orderPerDayUpperLimit,
                          "OrdersPerDayUpperLimit", bshouldDefault);

                setLimits(currentLimits, rl.singleOrderOpenQtyLimit, limits->m_defaultLimits.singleOrderOpenQtyLimit,
                          "OpenQuantity", bshouldDefault);

                setLimits(currentLimits, rl.singleTickerNetValueLimit, limits->m_defaultLimits.singleTickerNetValueLimit,
                          "PerTickerNetValue", bshouldDefault);

                setLimits(currentLimits, rl.m_singleTickerNetPositionInLotsLimit, limits->m_defaultLimits.m_singleTickerNetPositionInLotsLimit,
                          "PerTickerNetQuantity", bshouldDefault);

                rl.m_checkForExposure = currentLimits->getBool("EnableExposureCheck", false);

                setLimits(currentLimits, rl.m_tickerLevelLossWarningLimit, limits->m_defaultLimits.m_tickerLevelLossWarningLimit,
                          "InstrumentLevelLossWarningLimit", bshouldDefault);

                setLimits(currentLimits, rl.m_tickerLevelLossFatalLimit, limits->m_defaultLimits.m_tickerLevelLossFatalLimit,
                          "InstrumentLevelLossFatalLimit", bshouldDefault);

                setLimits(currentLimits, rl.m_orderPerTradeLimit, limits->m_defaultLimits.m_orderPerTradeLimit,
                          "OrdersPerTrade", bshouldDefault);

                setLimits(currentLimits, rl.m_orderPerTradeLowerLimit, limits->m_defaultLimits.m_orderPerTradeLowerLimit,
                          "OrdersPerTradeLowerLimit", bshouldDefault);

                setLimits(currentLimits, rl.m_orderPerTradeUpperLimit, limits->m_defaultLimits.m_orderPerTradeUpperLimit,
                          "OrdersPerTradeUpperLimit", bshouldDefault);

                setLimits(currentLimits, rl.m_transactionCostCalculator, limits->m_defaultLimits.m_transactionCostCalculator,
                          "TransactionCostCalculator", bshouldDefault);

                setLimits(currentLimits, rl.m_percentageAwayFromLTPLimit, limits->m_defaultLimits.m_percentageAwayFromLTPLimit,
                          "PercentageAwayFromLTPLimit", bshouldDefault);

                //                    rl.m_transactionCostCalculator = limits->m_defaultLimits.m_transactionCostCalculator;
                //                    if(rl.m_transactionCostCalculator == 0)
                //                    throw "Transaction cost calculator is not specified in the limits";

                setLimits(currentLimits, rl.m_buyTransactionCostFactorOnQty, limits->m_defaultLimits.m_buyTransactionCostFactorOnQty,
                          "BuyTransactionCostOnQtyFactor", bshouldDefault);

                //                    rl.m_buyTransactionCostFactorOnQty = limits->m_defaultLimits.m_buyTransactionCostFactorOnQty;
                //                    if(rl.m_buyTransactionCostFactorOnQty == 0)
                //                    throw "Buy transaction cost factor is not specified in the limits";

                setLimits(currentLimits, rl.m_sellTransactionCostFactorOnQty, limits->m_defaultLimits.m_sellTransactionCostFactorOnQty,
                          "SellTransactionCostOnQtyFactor", bshouldDefault);

                //                    rl.m_sellTransactionCostFactorOnQty = limits->m_defaultLimits.m_sellTransactionCostFactorOnQty;
                //                    if(rl.m_sellTransactionCostFactorOnQty == 0)
                //                    throw "Sell transaction cost factor is not specified in the limits";

                setLimits(currentLimits, rl.m_buyTransactionCostFactorOnValue, limits->m_defaultLimits.m_buyTransactionCostFactorOnValue,
                          "BuyTransactionCostOnValueFactor", bshouldDefault);

                //                    rl.m_buyTransactionCostFactorOnValue = limits->m_defaultLimits.m_buyTransactionCostFactorOnValue;
                //                    if(rl.m_buyTransactionCostFactorOnValue == 0)
                //                    throw "buyTransactionCostFactorOnValue is not specified in the limits";

                setLimits(currentLimits, rl.m_sellTransactionCostFactorOnValue, limits->m_defaultLimits.m_sellTransactionCostFactorOnValue,
                          "SellTransactionCostOnValueFactor", bshouldDefault);

                //                    rl.m_sellTransactionCostFactorOnValue = limits->m_defaultLimits.m_sellTransactionCostFactorOnValue;
                //                    if(rl.m_sellTransactionCostFactorOnValue == 0)
                //                    throw "sellTransactionCostFactorOnValue is not specified in the limits";

                setLimits(currentLimits, rl.m_maxCancelConfirmationLimit, limits->m_defaultLimits.m_maxCancelConfirmationLimit,
                          "MaxCancelConfirmationLimit", bshouldDefault);

                rl.m_accountLevelLossFatalLimit = limits->m_defaultLimits.m_accountLevelLossFatalLimit;
                if (rl.m_accountLevelLossFatalLimit == 0)
                    throw "Account level loss fatal limit is not specified in the limits";

                rl.m_accountLevelLossWarningLimit = limits->m_defaultLimits.m_accountLevelLossWarningLimit;
                if (rl.m_accountLevelLossWarningLimit == 0)
                    throw "Account level loss warning limit is not specified in the limits";

                auto instrIter = instrLimitIter->nextElement("Instrument");
                while (instrIter)
                {
                    std::string instrName = instrIter->getStringAttribute("name");
                    auto instrFound = limits->m_instrLimits.find(instrName);
                    if (instrFound == limits->m_instrLimits.end())
                    {
                        auto instrInsert = limits->m_instrLimits.insert(std::make_pair(instrName, rl));
                        instrFound = instrInsert.first;
                    }
                    else
                    {
                        RMSLimits &instrLevelLimits = instrFound->second;
                        instrLevelLimits = rl;
                    }

                    instrIter = instrIter->nextElement("Instrument");
                }
                instrLimitIter = instrLimitIter->parentElement()->nextElement("InstrumentLimits");
            }
            accountIter = accountIter->parentElement()->nextElement("Account");
        }
    }
};

#endif

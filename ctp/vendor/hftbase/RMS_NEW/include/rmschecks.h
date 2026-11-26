#ifndef _INFRA_RMSCHECKS_H
#define _INFRA_RMSCHECKS_H

#include "orderresponse.h"
#include "rmscommon.h"
#include "configreader.h"
#include "errorcodes.h"
#include "marketdeltaNew.h"
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <unordered_map>
#include "rmslimits.h"

namespace illuminati
{
    namespace infra
    {
        typedef std::function<bool(const RequestMsg &, OrderPositions &, int32_t &)> RULES;

        /*!
         * RuleTypes and string helper
         * Add new rules enum before MAX_RMS_RULES
         */
        enum RuleTypes
        {
            NO_ERROR,
            VALID_SYMBOL_CHECK,
            ORDER_LIMIT_CHECK,
            TICKER_LIMIT_CHECK,
            ORDER_FREQUENCY_CHECK,
            NEGATIVE_PRICE_OR_QUANTITY_CHECK,
            ORDERS_PER_DAY_CHECK,
            ORDERS_PER_TRADE_CHECK,
            STOP_LOSS_CHECK,
            ALLOWED_SYMBOL_CHECK,
            QUANTITY_FIRMLEVEL_CHECK,
            NET_POSITION_FIRMLEVEL_CHECK,
            X_PERCENT_AWAY_FROM_LTP_CHECK,
            KRX_ORDERS_PER_TRADE_CHECK,
            CANCEL_CONFIRMS_COUNT_CHECK,
            MAX_RMS_RULES
        };

        static const char *ruleNameFromType(RuleTypes type)
        {
            switch (type)
            {
            case NO_ERROR:
                return "NO_ERROR";
            case VALID_SYMBOL_CHECK:
                return "VALID_SYMBOL_CHECK";
            case ORDER_LIMIT_CHECK:
                return "ORDER_LIMIT_CHECK";
            case TICKER_LIMIT_CHECK:
                return "TICKER_LIMIT_CHECK";
            case ORDER_FREQUENCY_CHECK:
                return "ORDER_FREQUENCY_CHECK";
            case ORDERS_PER_DAY_CHECK:
                return "ORDERS_PER_DAY_CHECK";
            case ORDERS_PER_TRADE_CHECK:
                return "ORDERS_PER_TRADE_CHECK";
            case STOP_LOSS_CHECK:
                return "STOP_LOSS_CHECK";
            case ALLOWED_SYMBOL_CHECK:
                return "ALLOWED_SYMBOL_CHECK";
            case QUANTITY_FIRMLEVEL_CHECK:
                return "QUANTITY_FIRMLEVEL_CHECK";
            case NET_POSITION_FIRMLEVEL_CHECK:
                return "NET_POSITION_FIRMLEVEL_CHECK";
            case X_PERCENT_AWAY_FROM_LTP_CHECK:
                return "X_PERCENT_AWAY_FROM_LTP_CHECK";
            case KRX_ORDERS_PER_TRADE_CHECK:
                return "KRX_ORDERS_PER_TRADE_CHECK";
            case CANCEL_CONFIRMS_COUNT_CHECK:
                return "CANCEL_CONFIRMS_COUNT_CHECK";
            case NEGATIVE_PRICE_OR_QUANTITY_CHECK:
                return "NEGATIVE_PRICE_OR_QUANTITY_CHECK";
            default:
                return "RuleType not mapped to string";
            }
        }

        class RmsChecks
        {

        public:
            RmsChecks();
            void reset(RegisteredRMSAccountPosCache &accountPosStartUpCache, RMSInstrumentCache &instrDefCache,
                       RmsConfig rmsConfig);
            void lock() { m_lock.lock(); }
            void unlock() { m_lock.unlock(); }
            bool execute(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool execute(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum, RuleTypes ruleId);
            bool IsSymbolAllowed(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool IsSymbolEnabled(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool SingleOrderThresholdChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool SingleTickerThresholdChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool OrderFrequencyThresholdChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool OrderFrequencyCheckEnabled() const;
            double GetOrderFrequencyWindow() const;
            size_t GetOrderPerSecondLimit() const;
            bool OrdersPerDayThresholdChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool OrdersPerTradeThresholdChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool StopLossPerTickerCheck(double &tickerLoss, const RMSLimits *thresholds, int32_t &errorNum);
            bool StopLossAccountLevelCheck(double &accountLoss, const RMSLimits *thresholds, int32_t &errorNum);
            bool StopLossAccountLevelCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool StopLossCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool NetPositionFirmLevelCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool QuantityFirmLevelCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool XPercentAwayFromLTPCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool KRXTotalOrdersPerTradeUsingTradedQtyChecks(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool CancelConfirmationCountCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool NegativePriceOrQuantityCheck(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool IsCancelConfirmsCountCheckEnabled() const;
            OrderFreqModeCode GetOrderPerSecondCalcMode() const;

        private:
            bool setThresholdsForAccount(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            bool setThresholdsForInstrument(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            void setCurrentInstrumentDetails(const RequestMsg &req, OrderPositions &positions);
            bool setCurrentValuesFromCaches(const RequestMsg &req, OrderPositions &positions, int32_t &errorNum);
            double GetNetLossPerTicker(const TickerPositions &tickerPosition, double lastTrdPx);

            RULES m_AllRules[MAX_RMS_RULES];
            std::unordered_set<std::string> m_banndedSymbols;
            std::unordered_set<std::string> m_allowedSymbols;
            std::vector<RuleTypes> m_subscribedRules;
            OrderQueue m_orderQueue;
            illuminati::ds::CasLock m_lock;
            RegisteredRMSAccountPosCache *m_accountPosStartUpCache;
            RMSInstrumentCache *m_instrDefCache;
            RMSLimits *m_pThresholds;
            RMSLimits *m_firmLevelThresholds;
            rmsInstrumentDetails *m_instrDet;
            // some other global limits
            bool m_checkForOrderFrequency;
            size_t m_orderFrequencyLimit;
            double m_orderFrequencyWindow;
            RmsConfig m_rmsConfig;
            bool m_cancelConfirmsCountCheckEnabled;
            OrderFreqModeCode m_orderFreqCalcMode;
        };
    }
}

#endif

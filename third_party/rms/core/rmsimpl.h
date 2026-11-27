#ifndef _INFRA_RMSIMPL_NEW_H
#define _INFRA_RMSIMPL_NEW_H

#include <vector>
#include <map>
#include "rmschecks.h"
#include "rmscommon.h"
#include "orderresponse.h"
#include "marketupdateNew.h"
#include "signalcallback.h"
#include "rmslimits.h"
#include "infra/Metrics.h"
#include "infra/Metrics.h"

namespace ors
{
    namespace infra
    {
        enum CancelChecks
        {
            ORDER_FREQ_CXL_CHECK = 2,
            CONFIRM_COUNT_CXL_CHECK = 4
        };

        enum CancelCheckEnabled
        {
            NO_CHECK_ENABLED = 1,
            ORDER_FREQ_CHECK_ENABLED = 3,
            CONFIRM_CXL_COUNT_CHECK_ENABLED = 5,
            ALL_CHECK_ENABLED = 7
        };

        class DEFAULT_SYMBOL_NAMING_POLICY
        {
        public:
            static std::string getSymbolName(const ::ors::infra::RequestMsg &req)
            {
                return req.Contract_Description.Symbol;
            }
        };

        class RmsImpl
        {
        public:
            DEFINE_SIGNAL(RMS_ERROR, RMS_WARNING)
            SIGNAL_INIT(2)

            inline RmsImpl();
            inline ~RmsImpl();
            /*!
             * initRmsImpl
             */
            inline void initRmsImpl(int maxShmClients, std::string rmsConfig, std::string rmsFirmwideConfig = "",
                                    std::string rmsFirmiwdeConfigKey = "");

            /*!
             * reload method, call this to dynamically load the rms config  and rmschecks
             */
            inline void reset();

            /*!
             * @tparam T -> Should contain the public member
             * RoundLot   -- Quantities in a single lot for the symbol.
             * Factor     -- PriceFactor for the symbol.
             *
             * This method affects the following cases:
             *   1. Value Computations like:
             *        = request.Quantity * symbol.RoundLotForValueCalc * price * symbol.PriceFactor
             *          where:
             *                symbol.RoundLotForValueCalc is equal to RoundLot or 1 (if @param useRoundLotForValueCalc is false)
             *                symbol.PriceFactor is equal to Factor or 1 (if @param usePriceFactorForValueCalc is false)
             *
             *   2 Incoming request Quantity Checks like:
             *         RMSERR(eq.Quantity > m_pThresholds->singleOrderOpenQtyLimit * symbol.RoundLotForThresholdCheck)
             *         where:
             *              symbol.RoundLotForThresholdCheck is equal to RoundLot or 1( if @param applyRoundLotValueOnIncomingQuanityChecks if false)
             */

            template <typename T>
            void setExchangeInstrDefinitionMap(std::unordered_map<std::string, T> &xIntrDef,
                                               bool useRoundLotForValueCalc,
                                               bool usePriceFactorForValueCalc,
                                               bool applyRoundLotValueOnIncomingQuanityChecks)
            {
                ORS_LOG(INFO) << "RMS: Populating Exchange Instrument Definition Map " << std::endl;
                for (auto iter = xIntrDef.begin(); iter != xIntrDef.end(); iter++)
                {
                    std::string ticker = iter->first;
                    TickerPositions *tickPos = new TickerPositions(ticker);
                    tickPos->m_instrumentDetails.RoundLotForValueCalc = useRoundLotForValueCalc ? iter->second.RoundLot : 1.0;
                    tickPos->m_instrumentDetails.PriceFactor = usePriceFactorForValueCalc ? iter->second.Factor : 1.0;
                    tickPos->m_instrumentDetails.RoundLotForThresholdCheck = applyRoundLotValueOnIncomingQuanityChecks ? iter->second.RoundLot : 1.0;
                    m_instrDefCache.insert(std::make_pair(ticker, tickPos));
                    ORS_LOG(INFO) << "Instument name: " << ticker << "|"
                               << "RoundLotForValueCalc : " << tickPos->m_instrumentDetails.RoundLotForValueCalc << "|"
                               << "PriceFactor : " << tickPos->m_instrumentDetails.PriceFactor << std::endl;
                }
            }

            // Need to clean the above method and have a common one
            template <typename T>
            void setExchangeInstrDefinitionMap(std::unordered_map<std::string, T> &xIntrDef,
                                               bool useRoundLotForValueCalc,
                                               bool usePriceFactorForValueCalc,
                                               bool applyRoundLotValueOnIncomingQuanityChecks,
                                               bool useAdditionalData)
            {
                ORS_LOG(INFO) << "RMS: Populating Exchange Instrument Definition Map " << std::endl;
                for (auto iter = xIntrDef.begin(); iter != xIntrDef.end(); iter++)
                {
                    std::string ticker = iter->first;
                    TickerPositions *tickPos = new TickerPositions(ticker);
                    tickPos->m_instrumentDetails.RoundLotForValueCalc = useRoundLotForValueCalc ? iter->second.RoundLot : 1.0;
                    tickPos->m_instrumentDetails.PriceFactor = usePriceFactorForValueCalc ? iter->second.Factor : 1.0;
                    tickPos->m_instrumentDetails.RoundLotForThresholdCheck = applyRoundLotValueOnIncomingQuanityChecks ? iter->second.RoundLot : 1.0;
                    tickPos->m_instrumentDetails.AdditionalData = iter->second.AdditionalData;

                    m_instrDefCache.insert(std::make_pair(ticker, tickPos));
                    ORS_LOG(INFO) << "Instument name: " << ticker << "|"
                               << "RoundLotForValueCalc : " << tickPos->m_instrumentDetails.RoundLotForValueCalc << "|"
                               << "PriceFactor : " << tickPos->m_instrumentDetails.PriceFactor << std::endl;
                }
            }
            /*!
             * new changes
             */
            inline void handleMDUpdates(::ors::md::MarketUpdateNew *update);
            template <typename SYMBOL_NAMING_POLICY = DEFAULT_SYMBOL_NAMING_POLICY>
            inline bool handleORSRequests(::ors::infra::RequestMsg *reqMsg);
            inline void handleORSResponse(::ors::infra::ResponseMsg *respMsg);
            // void startAsync(ShmMgr* shmManager, ShmMgr::RespShmQ* responseQ);

            /*!
             * request methods
             */
            template <typename SYMBOL_NAMING_POLICY = DEFAULT_SYMBOL_NAMING_POLICY>
            bool executeNewOrderChecks(const ::ors::infra::RequestMsg &req);
            template <typename SYMBOL_NAMING_POLICY = DEFAULT_SYMBOL_NAMING_POLICY>
            bool executeModifyOrderChecks(const ::ors::infra::RequestMsg &req);
            template <typename SYMBOL_NAMING_POLICY = DEFAULT_SYMBOL_NAMING_POLICY>
            bool executeCancelOrderChecks(const ::ors::infra::RequestMsg &req);
            /*!
             * reply methods
             */
            inline void handleTradeConfirm(const ::ors::infra::ResponseMsg &, const std::string &ticker, char side);
            inline void handleCxlOrderConfirm(const ::ors::infra::ResponseMsg &);
            inline void handleNewOrderConfirm(const ::ors::infra::ResponseMsg &);
            inline void handleOrderReject(const ::ors::infra::ResponseMsg &);
            inline void handleOrderRejectFromMiddleSystem(const ResponseMsg &resp);
            inline void handleModifyOrderReject(const ResponseMsg &resp);
            inline void handleModifyOrderRejectFromMiddleSystem(const ResponseMsg &resp);
            inline void handleModifyConfirm(const ::ors::infra::ResponseMsg &);
            inline void handleCxlOrderRejectFromMiddleSystem(const ResponseMsg &resp);
            /*!
             * this function is for the downstream Cancel Reject state where we want to decrement the count of cancel requests
             * In many cases, the cancel requests need not be decremented on rejects. So, this function is only where it is explicitly
             * known that cancel reject should decrement the request counts
             */
            inline void handleCxlOrderReject(const ResponseMsg &resp);

            /*!
             * Market Data method
             */
            inline void updateLTPFromMD(const ::ors::md::MarketUpdateNew *mdUpdate);
            /*!
             * misc
             */
            inline bool checkIfOrderExists(const ::ors::infra::RequestMsg &);
            inline ::ors::infra::RequestType getLastRequestOutgoingState(const ::ors::infra::ResponseMsg &);
            inline void printOrderStats(const uint32_t orderId);
            inline void printAllPositions();

        protected:
            inline void loadAccountPosCacheAndRMSLimits();
            inline ::ors::infra::AccountPositions *getAccountPosFromCache(std::string &accountId);
            inline ::ors::infra::TickerPositions *getTickerPosFromCache(const std::string &ticker);
            inline void setAccountValuesInCorrectFormat(const std::string &input, std::string &output, int32_t len);
            template <typename SYMBOL_NAMING_POLICY>
            OrderPositions *getTradingStatSoFar(const ::ors::infra::RequestMsg &);
            inline void storeOrderStats(::ors::infra::OrderPositions &, const ::ors::infra::RequestMsg &);
            inline ::ors::infra::OrderPositions *getOrderPositions(uint32_t orderId);
            inline void incrementTradingStatsOnNewOrder(::ors::infra::OrderPositions &, const ::ors::infra::RequestMsg &);
            inline void incrementOrDecrementTradingStatsOnModifyOrder(::ors::infra::OrderPositions &, const ::ors::infra::RequestMsg &);
            inline bool checkAndUpdateTradingStatsOnCancel(const RequestMsg &req, int32_t &error, ResponseType &response);
            inline void updateOrderFrequency(OrderFrequency *orderPos, uint64_t orderTime);
            inline void cleanupRmsImpl();

            std::string m_rmsConfigFile;
            std::string m_rmsFirmwideConfigFile;
            std::string m_rmsFirmwideConfigKey;
            RmsConfig m_rmsConfig;
            RmsChecks m_rmsChecks;
            RegisteredRMSAccountPosCache m_accountPosStartUpCache;
            RMSInstrumentCache m_instrDefCache;
            std::mutex m_criticalSection;
            std::unordered_map<uint32_t, ::ors::infra::OrderPositions *> m_orderToPositions;
            std::map<std::string, ::ors::infra::TickerLTPUpdates *> m_tickerToLTUpdates;
            std::unordered_map<std::string, bool> m_ticketIsLTPSet;

            std::mutex m_syncLTPUpdates;
            OrderFrequency *m_orderFrequency;
            bool m_orderFrequencyEnabled;
            int32_t m_orderFrequencyWindow;
            size_t m_orderPerSecondLimit;
            int32_t m_rmsStateError;
            bool m_cancelConfirmsCountCheckEnabled;
            OrderFreqModeCode m_orderFreqCalcMode;
            ::ors::infra::OrderPositionsPool m_orderPositionsPool;
#ifdef _RMS_STATS
            int32_t NEW_ORDER_ONE;
            int32_t MODIFY_ORDER_ONE;
            int32_t CANCEL_ORDER_ONE;
            int32_t m_rmsStatResponse;
#endif
        };
    }
}

#endif

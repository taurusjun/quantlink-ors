#ifndef _INFRA_NETPOSITIONAGGREGATENEW_H
#define _INFRA_NETPOSITIONAGGREGATENEW_H

#include <unordered_set>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <set>
#include "orderresponse.h"
#include <mutex>
#include "circularqueue.hpp"
#include "rmslimits.h"
#include "HeapBaseStoragePolicy.h"
#include "defaultinstrdef.h"

namespace ors
{
    namespace infra
    {

#define FORTS_ORDER_LIMIT 301 // this should be one greater than the total number of orders that can be placed.

        typedef SimpleCircularQueue<double, storagepolicy::HeapStorage> OrderQueue;

        struct rmsInstrumentDetails
        {
            double RoundLotForValueCalc;
            double RoundLotForThresholdCheck;
            double PriceFactor;
            ::ors::DataAddendum AdditionalData;
        };

        /*!
         * TotalOrdersPerDay
         */
        class TotalOrdersPerDay
        {
        public:
            TotalOrdersPerDay() : m_totalTradedValue(0.0), m_totalTradeCount(0), m_totalNewOrdersSent(0), m_totalCxlOrdersSent(0) {}
            inline __attribute__((always_inline)) void incrementNewOrdersCount()
            {
                m_lock.lock();
                m_totalNewOrdersSent++;
                m_lock.unlock();
            }
            inline __attribute__((always_inline)) void incrementCxlOrdersCount()
            {
                m_lock.lock();
                m_totalCxlOrdersSent++;
                m_lock.unlock();
            }
            inline __attribute__((always_inline)) void decrementNewOrdersCount()
            {
                m_lock.lock();
                if (m_totalNewOrdersSent > 0)
                    m_totalNewOrdersSent--;
                m_lock.unlock();
            }
            inline __attribute__((always_inline)) void decrementCxlOrdersCount()
            {
                m_lock.lock();
                if (m_totalCxlOrdersSent > 0)
                    m_totalCxlOrdersSent--;
                m_lock.unlock();
            }
            inline __attribute__((always_inline)) void updateTotalTradedValue(double singleTradeValue)
            {
                m_lock.lock();
                m_totalTradedValue = m_totalTradedValue + singleTradeValue;
                m_totalTradeCount++;
                m_lock.unlock();
            }
            inline __attribute__((always_inline)) int64_t getNewOrdersCount()
            {
                return m_totalNewOrdersSent;
            }
            inline __attribute__((always_inline)) int64_t getCxlOrdersCount()
            {
                return m_totalCxlOrdersSent;
            }
            inline __attribute__((always_inline)) double getTotalTradedValue()
            {
                return m_totalTradedValue;
            }
            inline __attribute__((always_inline)) uint64_t getTotalTradeCount()
            {
                return m_totalTradeCount;
            }
            void lock()
            {
                m_lock.lock();
            }
            void unlock()
            {
                m_lock.unlock();
            }
            std::string toString()
            {
                std::stringstream css;
                css << "TTV:" << std::setprecision(12) << m_totalTradedValue
                    << ",TotalNew:" << m_totalNewOrdersSent << std::endl;
                return css.str();
            }

        private:
            double m_totalTradedValue;
            uint64_t m_totalTradeCount;
            uint64_t m_totalNewOrdersSent;
            uint64_t m_totalCxlOrdersSent;
            std::mutex m_lock;
        };

        /*!
         * OrderFrequency
         */
        struct OrderFrequency
        {
            OrderFrequency() : totalOrdersInSecond(0)
            {
            }
            OrderFrequency(size_t queueSize) : totalOrdersInSecond(0)
            {
                m_orderQueue.setSize(queueSize);
            }
            // uint64_t time_start;
            // uint64_t time_end;
            int32_t totalOrdersInSecond;
            OrderQueue m_orderQueue;

            double getStartTime()
            {
                return m_orderQueue.getFirstNodeValue();
            }
            double getEndTime()
            {
                return m_orderQueue.getLastNodeValue();
            }
            std::string toString()
            {
                std::stringstream css;
                double time_start = m_orderQueue.getFirstNodeValue();
                double time_end = m_orderQueue.getLastNodeValue();

                css << "Start:" << time_start << ","
                    << "End:" << time_end << ","
                    << "Orders:" << totalOrdersInSecond << std::endl;
                return css.str();
            }
        };

        /*!
         * TickerLTPUpdates
         */
        struct TickerLTPUpdates
        {

            TickerLTPUpdates() : ltpAlternative(0.0), lastTradePrice(0.0)
            {
            }

            TickerLTPUpdates(double ltp, double alternative) : ltpAlternative(alternative), lastTradePrice(ltp)
            {
            }

            ~TickerLTPUpdates()
            {
                std::cout << "destroying TickerLTPUpdates" << std::endl;
            }

            void lock()
            {
                m_lock.lock();
            }

            void unlock()
            {
                m_lock.unlock();
            }

            double get()
            {
                double retVal;
                lock();
                retVal = lastTradePrice;
                unlock();
                return retVal;
            }
            double ltpAlternative;
            double lastTradePrice;
            std::mutex m_lock;
        };

        typedef std::map<std::string, TickerLTPUpdates *>::iterator TICKLTPUPITER;
        /*!
         * TickerPositions
         */
        struct TickerPositions
        {

            TickerPositions(std::string name)
                : symbol(name),
                  type(XXX),
                  netBuyTradedValue(0.0),
                  netBuyTradedQty(0),
                  netSellTradedValue(0.0),
                  netSellTradedQty(0),
                  netBuyOpenQty(0),
                  netSellOpenQty(0),
                  strikePrice(0),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0),
                  lastUpdateSide('N'),
                  netPnl(0),
                  thresholds(0),
                  netBuyTransactionCost(0.0),
                  netSellTransactionCost(0.0),
                  totalCancelOrderConfirms(0),
                  netBuyModifySentButNotConfirmedQty(0),
                  netSellModifySentButNotConfirmedQty(0)
            {
            }

            TickerPositions(const TickerPositions *tickFromCache)
                : symbol(tickFromCache->symbol), type(XXX), netBuyTradedValue(0.0), netBuyTradedQty(0.0), netSellTradedValue(0.0), netSellTradedQty(0.0), netBuyOpenQty(0.0), netSellOpenQty(0.0), strikePrice(0), buyOpenOrdersCount(0), sellOpenOrdersCount(0), lastUpdateSide('N'), netPnl(0), thresholds(NULL), netBuyTransactionCost(0.0), netSellTransactionCost(0.0), totalCancelOrderConfirms(0), netBuyModifySentButNotConfirmedQty(0), netSellModifySentButNotConfirmedQty(0)
            {
                m_instrumentDetails.PriceFactor = tickFromCache->m_instrumentDetails.PriceFactor;
                m_instrumentDetails.RoundLotForThresholdCheck = tickFromCache->m_instrumentDetails.RoundLotForThresholdCheck;
                m_instrumentDetails.RoundLotForValueCalc = tickFromCache->m_instrumentDetails.RoundLotForValueCalc;
                m_instrumentDetails.AdditionalData = tickFromCache->m_instrumentDetails.AdditionalData;
                thresholds = tickFromCache->thresholds;
            }

            ~TickerPositions()
            {
                std::cout << "destroying TickerPositions" << std::endl;
            }

            bool operator==(const TickerPositions &rhs) const
            {
                return symbol == rhs.symbol;
            }

            void lock()
            {
                m_lock.lock();
            }

            void unlock()
            {
                m_lock.unlock();
            }

            std::string toString()
            {
                m_lock.lock();
                std::stringstream css;
                css << "PositionsForSymbol:" << symbol << ","
                    << "type:" << type << ","
                    << "netBuyTradedValue:" << netBuyTradedValue << ","
                    << "netBuyTradedQty:" << netBuyTradedQty << ","
                    << "netSellTradedValue:" << netSellTradedValue << ","
                    << "netSellTradedQty:" << netSellTradedQty << ","
                    << "netBuyOpenQty:" << netBuyOpenQty << ","
                    << "netSellOpenQty:" << netSellOpenQty << ","
                    << "netBuyModifySentButNotConfirmedQty:" << netBuyModifySentButNotConfirmedQty << ","
                    << "netSellModifySentButNotConfirmedQty:" << netSellModifySentButNotConfirmedQty << ","
                    << "buyOpenOrdersCount:" << buyOpenOrdersCount << ","
                    << "sellOpenOrdersCount:" << sellOpenOrdersCount << std::endl;
                m_lock.unlock();
                return css.str();
            }

            std::string symbol;
            rmsInstrumentDetails m_instrumentDetails;
            InstrumentType type;
            double netBuyTradedValue;
            int32_t netBuyTradedQty;
            double netSellTradedValue;
            int32_t netSellTradedQty;
            int32_t netBuyOpenQty;
            int32_t netSellOpenQty;
            int32_t netBuyModifySentButNotConfirmedQty;
            int32_t netSellModifySentButNotConfirmedQty;
            double strikePrice;
            uint32_t buyOpenOrdersCount;
            uint32_t sellOpenOrdersCount;
            unsigned char lastUpdateSide;
            int64_t netPnl;
            std::mutex m_lock;
            RMSLimits *thresholds;
            double netBuyTransactionCost;
            double netSellTransactionCost;
            TICKLTPUPITER tickLtpIterator;
            uint32_t totalCancelOrderConfirms;
        };

        typedef std::map<std::string, TickerPositions *>::iterator TICKERPOSITER;

        /*!
         * AccountPositions
         */
        struct AccountPositions
        {

            AccountPositions()
                : accountId(""),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0),
                  netBuyTradedQty(0),
                  netSellTradedQty(0),
                  netBuyTradedValue(0.0),
                  netSellTradedValue(0.0),
                  m_accountLimits(0)
            {
            }

            AccountPositions(const std::string &x)
                : accountId(x),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0),
                  netBuyTradedQty(0),
                  netSellTradedQty(0),
                  netBuyTradedValue(0.0),
                  netSellTradedValue(0.0),
                  m_accountLimits(0)
            {
            }

            ~AccountPositions()
            {
                std::cout << "destroying AccountPositions" << std::endl;
                for (auto it = perTickerPositions.begin(); it != perTickerPositions.end(); it++)
                {
                    if (it->second != NULL)
                        delete it->second;
                }

                if (perTickerPositions.size())
                    perTickerPositions.clear();
            }

            bool operator==(const AccountPositions &rhs) const
            {
                return accountId == rhs.accountId;
            }

            std::string toString()
            {
                std::stringstream css;
                css << "accountId:" << accountId << "|";
                for (auto iter = perTickerPositions.begin(); iter != perTickerPositions.end(); iter++)
                {
                    TickerPositions &tickPos = *(iter->second);
                    css << tickPos.toString();
                }
                return css.str();
            }

            TickerPositions *findTickerPositions(const std::string &ticker)
            {
                auto iter = perTickerPositions.find(ticker);
                if (iter == perTickerPositions.end())
                {
                    return NULL;
                }
                else
                {
                    return iter->second;
                }
            }

            inline __attribute__((always_inline)) void updateBuyTradeQtyAndValue(int32_t tradedQuantity, double currentTradedValue)
            {
                m_lock.lock();
                netBuyTradedQty += tradedQuantity;
                netBuyTradedValue += currentTradedValue;
                m_lock.unlock();
            }

            inline __attribute__((always_inline)) void updateSellTradeQtyAndValue(int32_t tradedQuantity, double currentTradedValue)
            {
                m_lock.lock();
                netSellTradedQty += tradedQuantity;
                netSellTradedValue += currentTradedValue;
                m_lock.unlock();
            }

            inline __attribute__((always_inline)) uint64_t getTotalTradedQty()
            {
                m_lock.lock();
                uint64_t totalTradedQty = netSellTradedQty + netBuyTradedQty;
                m_lock.unlock();
                return totalTradedQty;
            }

            void lock()
            {
                m_lock.lock();
            }

            void unlock()
            {
                m_lock.unlock();
            }

            std::string accountId;
            std::map<std::string, TickerPositions *> perTickerPositions;
            TotalOrdersPerDay ordersPerDay;
            uint32_t buyOpenOrdersCount;
            uint32_t sellOpenOrdersCount;
            double netBuyTradedQty;
            double netSellTradedQty;
            double netBuyTradedValue;
            double netSellTradedValue;
            std::mutex m_lock;
            std::mutex m_tickerPositionslock;
            RmsAccountLimits *m_accountLimits;
        };

        typedef std::unordered_map<std::string, AccountPositions *>::iterator ACCOUNTPOSITER;
        typedef std::unordered_map<std::string, ::ors::infra::AccountPositions *> RegisteredRMSAccountPosCache;
        typedef std::unordered_map<std::string, ::ors::infra::AccountPositions *>::iterator RegisteredRMSAccountPosCacheIter;

        /*!
         * StrategyPositions
         */
        struct StrategyPositions
        {

            StrategyPositions(int32_t id)
                : strategyId(id),
                  accountIterator(),
                  netBuyTradedQty(0),
                  netSellTradedQty(0),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0)
            {
            }

            ~StrategyPositions()
            {
                std::cout << "destroying StrategyPositions" << std::endl;
                /*for (auto it = perAccountPositions.begin(); it != perAccountPositions.end(); it++)
                {
                    if (it->second != NULL)
                    {
                        delete it->second;
                        it->second = NULL;
                    }
                }*/

                if (perAccountPositions.size())
                {
                    perAccountPositions.clear();
                }
            }

            bool operator==(const StrategyPositions &rhs) const
            {
                return strategyId == rhs.strategyId;
            }

            std::string toString()
            {
                std::stringstream css;
                css << "strategyId:" << strategyId << "|";
                for (auto iter = perAccountPositions.begin(); iter != perAccountPositions.end(); iter++)
                {
                    AccountPositions &accPos = *(iter->second);
                    css << accPos.toString();
                }
                return css.str();
            }

            inline __attribute__((always_inline)) TickerPositions *findTickerPositions(const std::string &symbol)
            {
                TickerPositions *tickPos = NULL;
                for (auto iter = perAccountPositions.begin(); iter != perAccountPositions.end(); iter++)
                {
                    AccountPositions &accPos = *(iter->second);
                    tickPos = accPos.findTickerPositions(symbol);
                    if (tickPos != NULL)
                        break;
                }
                return tickPos;
            }

            int32_t strategyId;
            ACCOUNTPOSITER accountIterator;
            double netBuyTradedQty;
            double netSellTradedQty;
            uint32_t buyOpenOrdersCount;
            uint32_t sellOpenOrdersCount;
            std::unordered_map<std::string, AccountPositions *> perAccountPositions;
        };
        /*!
         * OrderPositions
         */
        struct OrderPositions
        {

            OrderPositions() : stratPosition(NULL),
                               orderID(0),
                               openPrice(0.0),
                               openQty(0),
                               doneQty(0),
                               sentQty(0),
                               sentQtyDelta(0),
                               sentPrice(0.0),
                               side('N'),
                               confirmed(false),
                               active(false),
                               orderTime(0),
                               fepOriginalOrderId(0),
                               modifyCount(0),
                               modifySentButNotConfirmedQty(0),
                               previousModifySentButNotConfirmedQty(0)
            {
            }

            OrderPositions(uint32_t id) : stratPosition(NULL),
                                          orderID(id),
                                          openPrice(0.0),
                                          openQty(0),
                                          doneQty(0),
                                          sentQty(0),
                                          sentQtyDelta(0),
                                          sentPrice(0.0),
                                          side('N'),
                                          confirmed(false),
                                          active(false),
                                          orderTime(0),
                                          fepOriginalOrderId(0),
                                          modifyCount(0),
                                          modifySentButNotConfirmedQty(0),
                                          previousModifySentButNotConfirmedQty(0)
            {
            }

            inline __attribute__((always_inline)) void close()
            {
                active = false;
            }

            inline __attribute__((always_inline)) void confirm()
            {
                openQty = sentQty;
                openPrice = sentPrice;
                sentPrice = 0.0;
                sentQty = 0;
                sentQtyDelta = 0;
                confirmed = true;
            }

            inline __attribute__((always_inline)) void unconfirmNewOrder()
            {
                sentQty = openQty;
                sentPrice = openPrice;
                openQty = 0;
                openPrice = 0;
                sentQtyDelta = 0;
                confirmed = false;
            }

            void reject()
            {
                sentQty = 0;
                sentPrice = 0;
                confirmed = true;
            }

            int32_t getMaxQuantityFromSentButNotConfirmedQueue()
            {

                if (modifySentButNotConfirmedQtyMaxPriorityQueue.empty())
                    return 0;
                else
                    return (modifySentButNotConfirmedQtyMaxPriorityQueue.begin()->first);
            }

            void insertIntoSentButNotConfirmedPriorityQueue(int32_t quantity)
            {
                if (modifySentButNotConfirmedQtyMaxPriorityQueue.find(quantity) == modifySentButNotConfirmedQtyMaxPriorityQueue.end())
                    modifySentButNotConfirmedQtyMaxPriorityQueue[quantity] = 1;
                else
                    modifySentButNotConfirmedQtyMaxPriorityQueue[quantity]++;
            }

            void removeFromSentButNotConfirmedPriorityQueue(int32_t quantity)
            {
                auto it = modifySentButNotConfirmedQtyMaxPriorityQueue.find(quantity);
                (it->second)--;
                if (it->second == 0)
                {
                    modifySentButNotConfirmedQtyMaxPriorityQueue.erase(it);
                }
            }

            void lock()
            {
                m_lock.lock();
            }

            void unlock()
            {
                m_lock.unlock();
            }

            std::string toString()
            {
                m_lock.lock();
                std::stringstream css;
                css << "PositionsForOrderId:" << orderID
                    << ",openPrice:" << openPrice << ","
                    << "openQty:" << openQty << ","
                    << "modifySentButNotConfirmedQty:" << modifySentButNotConfirmedQty << ","
                    << "previousModifySentButNotConfirmedQty:" << modifySentButNotConfirmedQty << ","
                    << "doneQty:" << doneQty << ","
                    << "sentQty:" << sentQty << ","
                    << "sentPrice:" << sentPrice << ","
                    << "side:" << side << ","
                    << "confirmed:" << confirmed << ","
                    << "active:" << active
                    << "|";
                TickerPositions &pos = *(tickerIterator->second);
                css << pos.toString();
                m_lock.unlock();
                return css.str();
            }

            StrategyPositions *stratPosition;
            OrderFrequency *ordersPerSecond;
            ACCOUNTPOSITER accountIterator;
            TICKERPOSITER tickerIterator;
            std::mutex m_lock;
            RegisteredRMSAccountPosCache *allAccountsPointer;

            uint32_t orderID;
            double openPrice;
            int32_t openQty;
            int32_t modifySentButNotConfirmedQty;
            int32_t previousModifySentButNotConfirmedQty;
            std::queue<int32_t> modifySentButNotConfirmedQtyQueue;
            std::map<int32_t, int32_t, std::greater<int>> modifySentButNotConfirmedQtyMaxPriorityQueue;
            int32_t doneQty;
            int32_t sentQty;
            int32_t sentQtyDelta;
            double sentPrice;
            unsigned char side;
            bool confirmed;
            bool active;
            uint64_t orderTime;
            ::ors::infra::RequestType lastRequestOutgoingRequest;
            uint64_t fepOriginalOrderId;
            uint64_t modifyCount;
        };

        struct OrderPositionsPool
        {
            OrderPositionsPool()
                : free_objects_pool_(), m_allocPosition(0), m_size(0)
            {
                free_objects_pool_.reserve(1000);
            }

            void allocate(int size = 10000)
            {
                m_size = size;
                for (int i = 0; i < size; i++)
                {
                    OrderPositions *p = new OrderPositions(0);
                    free_objects_pool_.push_back(p);
                }
            }

            inline __attribute__((always_inline)) OrderPositions *getObject(uint32_t id)
            {
                m_lock.lock();

                if (m_allocPosition < m_size)
                {
                    OrderPositions *p = free_objects_pool_[m_allocPosition];
                    m_allocPosition++;
                    m_lock.unlock();
                    p->orderID = id;
                    return p;
                }

                m_lock.unlock();
                OrderPositions *p = new OrderPositions(id);
                return p;
            }

            inline __attribute__((always_inline)) void addObjectToPool()
            {
                OrderPositions *p = new OrderPositions(0);
                m_lock.lock();
                if (m_allocPosition > 0)
                {
                    --m_allocPosition;
                }
                // Push this object to the vector just like moving it to top of a stack
                free_objects_pool_[m_allocPosition] = p;
                m_lock.unlock();
            }

            // std::queue<OrderPositions * > free_objects_pool_;
            std::vector<OrderPositions *> free_objects_pool_;
            std::mutex m_lock;
            int32_t m_allocPosition;
            int32_t m_size;
        };

    }
}

#endif

#ifndef _INFRA_NETPOSITIONAGGREGATE_H
#define _INFRA_NETPOSITIONAGGREGATE_H

#include <unordered_set>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include "orderresponse.h"
#include "caslock.h"
#include "circularqueue.hpp"
#include "ArrayBasedStoragePolicy.h"

namespace illuminati
{
    namespace infra
    {

#define FORTS_ORDER_LIMIT 301 // this should be one greater than the total number of orders that can be placed.

        typedef SimpleCircularQueue<uint64_t, storagepolicy::ArrayStorage, INT(FORTS_ORDER_LIMIT)> OrderQueue;

        /*!
         * TotalOrdersPerDay
         */
        class TotalOrdersPerDay
        {
        public:
            TotalOrdersPerDay() : m_totalTradedValue(0.0), m_totalNewOrdersSent(0) {}
            void incrementNewOrdersCount()
            {
                m_lock.lock();
                m_totalNewOrdersSent++;
                m_lock.unlock();
            }
            void updateTotalTradedValue(double singleTradeValue)
            {
                m_lock.lock();
                m_totalTradedValue = m_totalTradedValue + singleTradeValue;
                m_lock.unlock();
            }
            int64_t getNewOrdersCount()
            {
                return m_totalNewOrdersSent;
            }
            double getTotalTradedValue()
            {
                return m_totalTradedValue;
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
            uint64_t m_totalNewOrdersSent;
            illuminati::ds::CasLock m_lock;
        };

        /*!
         * OrderFrequency
         */
        struct OrderFrequency
        {
            OrderFrequency() : totalOrdersInSecond(0)
            {
            }
            // uint64_t time_start;
            // uint64_t time_end;
            int32_t totalOrdersInSecond;
            OrderQueue m_orderQueue;

            uint64_t getStartTime()
            {
                return m_orderQueue.getFirstNodeValue();
            }
            uint64_t getEndTime()
            {
                return m_orderQueue.getLastNodeValue();
            }
            std::string toString()
            {
                std::stringstream css;
                uint64_t time_start = m_orderQueue.getFirstNodeValue();
                uint64_t time_end = m_orderQueue.getLastNodeValue();

                css << "Start:" << time_start << ","
                    << "End:" << time_end << ","
                    << "Orders:" << totalOrdersInSecond << std::endl;
                return css.str();
            }
        };

        /*!
         * TickerPositions
         */
        struct TickerPositions
        {

            TickerPositions(std::string name)
                : symbol(name),
                  netBuyTradedValue(0.0),
                  netBuyTradedQty(0.0),
                  netSellTradedValue(0.0),
                  netSellTradedQty(0.0),
                  netBuyOpenQty(0.0),
                  netSellOpenQty(0.0),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0)
            {
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
                css << "symbol:" << symbol << ","
                    << "type:" << type << ","
                    << "netBuyTradedValue:" << netBuyTradedValue << ","
                    << "netBuyTradedQty:" << netBuyTradedQty << ","
                    << "netSellTradedValue:" << netSellTradedValue << ","
                    << "netSellTradedQty:" << netSellTradedQty << ","
                    << "netBuyOpenQty:" << netBuyOpenQty << ","
                    << "netSellOpenQty:" << netSellOpenQty << ","
                    << "buyOpenOrdersCount:" << buyOpenOrdersCount << ","
                    << "sellOpenOrdersCount:" << sellOpenOrdersCount << std::endl;
                m_lock.unlock();
                return css.str();
            }

            std::string symbol;
            InstrumentType type;
            double netBuyTradedValue;
            double netBuyTradedQty;
            double netSellTradedValue;
            double netSellTradedQty;
            double netBuyOpenQty;
            double netSellOpenQty;
            double strikePrice;
            uint32_t buyOpenOrdersCount;
            uint32_t sellOpenOrdersCount;
            unsigned char lastUpdateSide;
            int64_t netPnl;
            illuminati::ds::CasLock m_lock;
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
                  netBuyTradedQty(0.0),
                  netSellTradedQty(0.0),
                  netBuyTradedValue(0.0),
                  netSellTradedValue(0.0)
            {
            }

            AccountPositions(const std::string &x)
                : accountId(x),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0),
                  netBuyTradedQty(0.0),
                  netSellTradedQty(0.0),
                  netBuyTradedValue(0.0),
                  netSellTradedValue(0.0)
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
            std::string accountId;
            std::map<std::string, TickerPositions *> perTickerPositions;
            uint32_t buyOpenOrdersCount;
            uint32_t sellOpenOrdersCount;
            double netBuyTradedQty;
            double netSellTradedQty;
            double netBuyTradedValue;
            double netSellTradedValue;

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
        };

        typedef std::map<std::string, AccountPositions *>::iterator ACCOUNTPOSITER;

        /*!
         * StrategyPositions
         */
        struct StrategyPositions
        {

            StrategyPositions(int32_t id)
                : strategyId(id),
                  netBuyTradedQty(0.0),
                  netSellTradedQty(0.0),
                  buyOpenOrdersCount(0),
                  sellOpenOrdersCount(0)
            {
            }

            ~StrategyPositions()
            {
                std::cout << "destroying StrategyPositions" << std::endl;
                for (auto it = perAccountPositions.begin(); it != perAccountPositions.end(); it++)
                {
                    if (it->second != NULL)
                        delete it->second;
                }

                if (perAccountPositions.size())
                {
                    perAccountPositions.clear();
                }
            }

            int32_t strategyId;
            double netBuyTradedQty;
            double netSellTradedQty;
            uint32_t buyOpenOrdersCount;
            uint32_t sellOpenOrdersCount;
            std::map<std::string, AccountPositions *> perAccountPositions;

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

            TickerPositions *findTickerPositions(const std::string &symbol)
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
        };

        /*!
         * TickerLTPUpdates
         */
        struct TickerLTPUpdates
        {

            TickerLTPUpdates() : lastTradePrice(0.0)
            {
            }

            TickerLTPUpdates(double x) : lastTradePrice(x)
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
            double lastTradePrice;
            illuminati::ds::CasLock m_lock;
        };

        typedef std::map<std::string, TickerLTPUpdates *>::iterator TICKLTPUPITER;

        /*!
         * OrderPositions
         */
        struct OrderPositions
        {

            OrderPositions() : stratPosition(NULL), accountIterator(NULL), tickerIterator(NULL), tickLtpIterator(NULL),
                               orderID(0),
                               openPrice(0.0),
                               openQty(0.0),
                               doneQty(0.0),
                               sentQty(0.0),
                               sentQtyDelta(0.0),
                               sentPrice(0.0),
                               side('N'),
                               confirmed(false),
                               active(false),
                               orderTime(0)
            {
            }

            OrderPositions(uint32_t id) : stratPosition(NULL), accountIterator(NULL), tickerIterator(NULL), tickLtpIterator(NULL),
                                          orderID(id),
                                          openPrice(0.0),
                                          openQty(0.0),
                                          doneQty(0.0),
                                          sentQty(0.0),
                                          sentQtyDelta(0.0),
                                          sentPrice(0.0),
                                          side('N'),
                                          confirmed(false),
                                          active(false),
                                          orderTime(0)
            {
            }

            void close()
            {
                active = false;
            }

            void confirm()
            {
                openQty = sentQty;
                openPrice = sentPrice;
                sentPrice = 0.0;
                sentQty = 0.0;
                sentQtyDelta = 0.0;
                confirmed = true;
            }

            void reject()
            {
                sentQty = 0;
                sentPrice = 0;
                confirmed = true;
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
                css << "Positions for orderId = " << orderID
                    << ":openPrice:" << openPrice << ","
                    << "openQty:" << openQty << ","
                    << "doneQty:" << doneQty << ","
                    << "sentQty:" << sentQty << ","
                    << "sentPrice:" << sentPrice << ","
                    << "side:" << side << ","
                    << "confirmed:" << confirmed << ","
                    << "active:" << active
                    << ",Pos on all sym so far: ";
                TickerPositions &pos = *(tickerIterator->second);
                css << pos.toString();
                m_lock.unlock();
                return css.str();
            }

            StrategyPositions *stratPosition;
            OrderFrequency *ordersPerSecond;
            TotalOrdersPerDay *ordersPerDay;
            ACCOUNTPOSITER accountIterator;
            TICKERPOSITER tickerIterator;
            TICKLTPUPITER tickLtpIterator;
            illuminati::ds::CasLock m_lock;

            uint32_t orderID;
            double openPrice;
            double openQty;
            double doneQty;
            double sentQty;
            double sentQtyDelta;
            double sentPrice;
            unsigned char side;
            bool confirmed;
            bool active;
            uint64_t orderTime;
        };
    }
}

#endif

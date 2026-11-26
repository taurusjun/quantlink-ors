#ifndef MARKETUPDATEONNETWORK_H_
#define MARKETUPDATEONNETWORK_H_

#include <inttypes.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "logger.h"
#include "marketupdateNew.h"
#include "itimer.h"

#pragma pack(push, 1)
namespace illuminati
{
    namespace md
    {
        const int32_t INTEREST_LEVELS_NET = 5;
        const int32_t MAX_SYMBOL_SIZE_NET = 32;

        /* m_feedType bitset values*/
        const uint32_t FEED_TBT_ON = 0;
        const uint32_t FEED_SNAPSHOT_ON = 1;

        /* m_side bitset values*/
        const uint32_t SIDE_BUY_ON = 0;
        const uint32_t SIDE_SELL_ON = 1;
        const uint32_t SIDE_SHORT_SELL_ON = 2;
        const uint32_t SIDE_NONE_ON = 3; // Can be used for Snapshot

        /* m_updateType bitset values*/
        const uint32_t MDUPDTYPE_ADD_ON = 0;           // new update
        const uint32_t MDUPDTYPE_MODIFY_ON = 1;        // modify update
        const uint32_t MDUPDTYPE_DELETE_ON = 2;        // delete/cancel update
        const uint32_t MDUPDTYPE_ORDER_ENTRY_ON = 3;   // can be used in snapshot
        const uint32_t MDUPDTYPE_TRADE_ON = 4;         // Exchange sent Trade plus an orderbook update
        const uint32_t MDUPDTYPE_TRADE_INFO_ON = 5;    // Exchange sent Trade Info
        const uint32_t MDUPDTYPE_TRADE_IMPLIED_ON = 6; // Predicted Trade plus an order book update
        const uint32_t MDUPDTYPE_NONE_ON = 7;          // Undefined m_updateType

        /* m_endPkt bitset values*/
        const uint32_t END_PKT_ZERO = 0;
        const uint32_t END_PKT_ONE = 1;
        const uint32_t END_PKT_TWO = 2; // Added for NSE MD to indicate last trade

        static uint64_t s_epochTillMidnightMs =
            illuminati::ITime_ClockRT::GetSecondsFromEpochTillMidnight() * 1000LL;

        struct bookElementNetwork_t
        {
            int32_t quantity;
            double price;

            std::string toString() const
            {
                std::ostringstream ss;
                ss << std::setw(12) << std::setprecision(10) << price << std::setw(10)
                   << quantity;

                return ss.str();
            }

            void print() const
            {
                std::cout << toString();
            }
        };

        struct MarketUpdateOverNetHeader
        {

            struct
            {
                uint32_t m_validLevels : 4;
                uint32_t m_updateLevel : 3; // puts constraint of not sending more than 8 levels
                uint32_t m_tradeFlag : 1;
                uint32_t m_updateType : 3;
                uint32_t m_side : 2;
                uint32_t m_endPkt : 2;
                uint32_t m_feedType : 1; // 16 bits
            };

            unsigned char m_exchangeName;

            uint64_t m_rptseqnum;

#ifdef ENABLE_SEQ_NUM
            uint64_t m_seqnum;
#endif

#ifdef ENABLE_OLD_PRICE_QUANT
            double m_oldPrice;
            int32_t m_oldQuant;
#endif

            uint8_t m_symbolLen;
        };

        struct TradeInfoON
        {
            uint32_t m_totalTradedQuantity;
            uint64_t m_totalTradedValue;
        };

        struct MarketUpdateSnapshotHeader
        {

            struct
            {
                uint8_t m_validBids : 4;
                uint8_t m_validAsks : 4;
            };
            double m_lastTradedPrice;
        };

        struct MarketUpdateOverNet : MarketUpdateOverNetHeader,
                                     MarketUpdateSnapshotHeader,
                                     TradeInfoON
        {

            char m_symbol[MAX_SYMBOL_SIZE_NET];

            union
            {
                struct
                {
                    bookElementNetwork_t m_AskBook[INTEREST_LEVELS_NET];
                    bookElementNetwork_t m_BidBook[INTEREST_LEVELS_NET];
                };
                bookElementNetwork_t m_book[INTEREST_LEVELS_NET];
            };

            double m_newPrice;
            int32_t m_newQuant;
            uint64_t m_localTs;

            inline __attribute__((always_inline)) void setTradeFlag(int val)
            {
                m_tradeFlag = val;
            }

            template <typename T1, typename T2>
            inline __attribute__((always_inline)) void copyBook(T1 &dest, const T2 &src)
            {
                dest.quantity = src.quantity;
                dest.price = src.price;
            }

            inline __attribute__((always_inline)) void copyFrom(const MarketUpdateNew &up)
            {
                m_rptseqnum = up.m_rptseqnum;
                m_newPrice = up.m_newPrice;
                m_newQuant = up.m_newQuant;
#ifdef ENABLE_SEQ_NUM
                m_seqnum = up.m_seqnum;
#endif

#ifdef ENABLE_OLD_PRICE_QUANT
                m_oldPrice = up.m_oldPrice;
                m_oldQuant = up.m_oldQuant;
#endif
                m_exchangeName = up.m_exchangeName;
                memcpy(m_symbol, up.m_symbol, sizeof(m_symbol));
                m_symbolLen = strlen(up.m_symbol);
                m_symbol[m_symbolLen] = '\0'; // just make sure that its null terminated
                m_feedType = convertFeedTypeToBits(up.m_feedType);

                if (up.m_feedType == FEED_TBT)
                {
                    if (illuminati::md::SIDE_BUY == up.m_side)
                    {
                        // copy bid book, with array bounds check
                        m_validLevels =
                            (up.m_validBids > INTEREST_LEVELS_NET) ? INTEREST_LEVELS_NET : up.m_validBids;
                        for (int cnt = 0; cnt < m_validLevels; cnt++)
                        {
                            copyBook(m_book[cnt], up.m_bidUpdates[cnt]);
                        }
                    }
                    else
                    {
                        // copy ask book, with array bounds check
                        m_validLevels =
                            (up.m_validAsks > INTEREST_LEVELS_NET) ? INTEREST_LEVELS_NET : up.m_validAsks;
                        for (int cnt = 0; cnt < m_validLevels; cnt++)
                        {
                            copyBook(m_book[cnt], up.m_askUpdates[cnt]);
                        }
                    }
                }
                else
                {

                    m_validBids =
                        (up.m_validBids > INTEREST_LEVELS_NET) ? INTEREST_LEVELS_NET : up.m_validBids;
                    m_validAsks =
                        (up.m_validAsks > INTEREST_LEVELS_NET) ? INTEREST_LEVELS_NET : up.m_validAsks;

                    for (int cnt = 0; cnt < m_validBids; cnt++)
                    {
                        copyBook(m_BidBook[cnt], up.m_bidUpdates[cnt]);
                    }
                    for (int cnt = 0; cnt < m_validAsks; cnt++)
                    {
                        copyBook(m_AskBook[cnt], up.m_askUpdates[cnt]);
                    }
                }
                m_totalTradedQuantity = up.m_totalTradedQuantity;
                m_totalTradedValue = up.m_totalTradedValue;
                m_lastTradedPrice = up.m_lastTradedPrice;
                m_tradeFlag = (up.m_tokenId == 1) ? 1 : 0;

                // set the other bit fields
                m_updateLevel = up.m_updateLevel;
                m_side = convertSideToBits(up.m_side);
                m_updateType = convertUpdateTypeToBits(up.m_updateType);
                m_endPkt = convertEndPktToBits(up.m_endPkt);
            }

            template <int InterestLevels>
            void copyFrom(const TMarketUpdateNew<InterestLevels> &up)
            {
                m_rptseqnum = up.m_rptseqnum;
                m_newPrice = up.m_newPrice;
                m_newQuant = up.m_newQuant;

#ifdef ENABLE_SEQ_NUM
                m_seqnum = up.m_seqnum;
#endif

#ifdef ENABLE_OLD_PRICE_QUANT
                m_oldPrice = up.m_oldPrice;
                m_oldQuant = up.m_oldQuant;
#endif

                m_exchangeName = up.m_exchangeName;
                memcpy(m_symbol, up.m_symbol, sizeof(m_symbol));
                m_symbolLen = strlen(up.m_symbol);
                m_symbol[m_symbolLen] = '\0'; // just make sure that its null terminated

                if (up.m_feedType == FEED_TBT)
                {
                    if (illuminati::md::SIDE_BUY == up.m_side)
                    {
                        // copy bid book, with array bounds check
                        m_validLevels = std::min(InterestLevels,
                                                 std::min((int)up.m_validBids,
                                                          (int)INTEREST_LEVELS_NET));
                        for (int cnt = 0; cnt < m_validLevels; cnt++)
                        {
                            copyBook(m_book[cnt], up.m_bidUpdates[cnt]);
                        }
                    }
                    else
                    {
                        // copy ask book, with array bounds check
                        m_validLevels = std::min(InterestLevels,
                                                 std::min((int)up.m_validAsks,
                                                          (int)INTEREST_LEVELS_NET));
                        for (int cnt = 0; cnt < m_validLevels; cnt++)
                        {
                            copyBook(m_book[cnt], up.m_askUpdates[cnt]);
                        }
                    }
                }
                else
                {

                    m_validBids = std::min(InterestLevels,
                                           std::min((int)up.m_validBids, (int)INTEREST_LEVELS_NET));
                    m_validAsks = std::min(InterestLevels,
                                           std::min((int)up.m_validAsks, (int)INTEREST_LEVELS_NET));

                    for (int cnt = 0; cnt < m_validBids; cnt++)
                    {
                        copyBook(m_BidBook[cnt], up.m_bidUpdates[cnt]);
                    }
                    for (int cnt = 0; cnt < m_validAsks; cnt++)
                    {
                        copyBook(m_AskBook[cnt], up.m_AskUpdates[cnt]);
                    }
                }
                m_totalTradedQuantity = up.m_totalTradedQuantity;
                m_totalTradedValue = up.m_totalTradedValue;
                m_lastTradedPrice = up.m_lastTradedPrice;
                m_tradeFlag = (up.m_tokenId == 1) ? 1 : 0;

                // set the other bit fields
                m_updateLevel = up.m_updateLevel;
                m_side = convertSideToBits(up.m_side);
                m_updateType = convertUpdateTypeToBits(up.m_updateType);
                m_endPkt = convertEndPktToBits(up.m_endPkt);
            }

            inline __attribute__((always_inline)) void copyTo(MarketUpdateNew &up)
            {
                up.m_rptseqnum = m_rptseqnum;
                up.m_newPrice = m_newPrice;
                up.m_newQuant = m_newQuant;

#ifdef ENABLE_SEQ_NUM
                up.m_seqnum = m_seqnum;
#endif

#ifdef ENABLE_OLD_PRICE_QUANT
                up.m_oldPrice = m_oldPrice;
                up.m_oldQuant = m_oldQuant;
#endif

                up.m_exchangeName = m_exchangeName;
                memcpy(up.m_symbol, m_symbol,
                       sizeof(m_symbol) < sizeof(up.m_symbol) ? sizeof(m_symbol) : sizeof(up.m_symbol));
                up.m_updateLevel = m_updateLevel;
                up.m_endPkt = getEndPktFromBits(m_endPkt);
                up.m_side = getSideFromBits(m_side);
                up.m_updateType = getUpdateTypeFromBits(m_updateType);

                if (m_feedType == FEED_TBT_ON)
                {

                    if (!(m_updateType == md::MDUPDTYPE_TRADE_INFO_ON || m_updateType == md::MDUPDTYPE_TRADE_IMPLIED_ON))
                    {
                        if (illuminati::md::SIDE_BUY == up.m_side)
                        {
                            up.m_validBids = m_validLevels;
                            for (int cnt = 0; cnt < m_validLevels; cnt++)
                            {
                                copyBook(up.m_bidUpdates[cnt], m_book[cnt]);
                            }
                        }
                        else if (illuminati::md::SIDE_SELL == up.m_side)
                        {
                            up.m_validAsks = m_validLevels;
                            for (int cnt = 0; cnt < m_validLevels; cnt++)
                            {
                                copyBook(up.m_askUpdates[cnt], m_book[cnt]);
                            }
                        }
                        else
                        {
                            // SHOULD NEVER COME HERE
                            ILOG(FATAL) << "Unknown side detected in network packet" << "\n";
                        }
                    }
                    up.m_feedType = FEED_TBT;
                }
                else
                {

                    up.m_validBids = m_validBids;
                    for (int cnt = 0; cnt < m_validBids; cnt++)
                    {
                        copyBook(up.m_bidUpdates[cnt], m_BidBook[cnt]);
                    }

                    up.m_validAsks = m_validAsks;
                    for (int cnt = 0; cnt < m_validAsks; cnt++)
                    {
                        copyBook(up.m_askUpdates[cnt], m_AskBook[cnt]);
                    }

                    up.m_feedType = FEED_SNAPSHOT;
                }
                up.m_totalTradedQuantity = m_totalTradedQuantity;
                up.m_totalTradedValue = m_totalTradedValue;
                up.m_lastTradedPrice = m_lastTradedPrice;
            }

            std::string toString(void)
            {
                std::ostringstream ss;
                ss
                    << "|Symbol=" << std::string(m_symbol, m_symbolLen)
                    << "| Symbol length=" << (int)m_symbolLen
#ifdef ENABLE_SEQ_NUM
                    << "|Seqnum=" << m_seqnum
#endif
                    << "| feedtype=" << m_feedType
                    << "|RptSeqnum=" << m_rptseqnum
                    << "|localTS=" << m_localTs
                    << "|ExchNameCode=" << (int)m_exchangeName
                    << "|updateType=" << getUpdateTypeFromBits(m_updateType)
                    << "|newPrice=" << std::setprecision(10) << m_newPrice
                    << "|newQuantity=" << m_newQuant
#ifdef ENABLE_OLD_PRICE_QUANT
                    << "|oldPrice=" << std::setprecision(10) << m_oldPrice
                    << "|oldQuantity=" << m_oldQuant
#endif
                    << "|updateLevel=" << (int)m_updateLevel
                    << "|endPacket=" << getEndPktFromBits(m_endPkt)
                    << "|side=" << getSideFromBits(m_side);

                ss << "|TotalTradedQuantity=" << m_totalTradedQuantity
                   << "|TotalTradedValue=" << m_totalTradedValue
                   << "|ValidAsks=" << (int)m_validAsks
                   << "|ValidBids=" << (int)m_validBids;

                ss << "\nAsk:";
                for (int i = 0; i < m_validAsks; i++)
                {
                    ss << "|P=" << std::setprecision(10) << m_AskBook[i].price << "|Q=" << m_AskBook[i].quantity;
                }

                ss << "\nBid:";
                for (int i = 0; i < m_validBids; i++)
                {
                    ss << "|P=" << std::setprecision(10) << m_BidBook[i].price << "|Q=" << m_BidBook[i].quantity;
                }

                ss << "|ValidLevels=" << (int)m_validLevels
                   << "\nBook:";
                for (int i = 0; i < INTEREST_LEVELS_NET; i++)
                {
                    ss << "|P=" << std::setprecision(10) << m_book[i].price << "|Q=" << m_book[i].quantity;
                }
                ss << "\n";
                return ss.str();
            }

        private:
            // side bit conversion
            inline __attribute__((always_inline))
            uint32_t
            convertSideToBits(unsigned char side)
            {
                switch (side)
                {
                case SIDE_BUY:
                    return SIDE_BUY_ON;
                case SIDE_SELL:
                    return SIDE_SELL_ON;
                case SIDE_SHORT_SELL:
                    return SIDE_SHORT_SELL_ON;
                case SIDE_NONE:
                    return SIDE_NONE_ON;
                default:
                    return SIDE_NONE_ON;
                }
            }
            // getting side from the bits
            inline __attribute__((always_inline)) unsigned char getSideFromBits(uint32_t sideBits)
            {
                switch (sideBits)
                {
                case SIDE_BUY_ON:
                    return SIDE_BUY;
                case SIDE_SELL_ON:
                    return SIDE_SELL;
                case SIDE_SHORT_SELL_ON:
                    return SIDE_SHORT_SELL;
                case SIDE_NONE_ON:
                    return SIDE_NONE;
                default:
                    return SIDE_NONE;
                }
            }

            // feedType bit conversion
            inline __attribute__((always_inline))
            uint32_t
            convertFeedTypeToBits(unsigned char feedType)
            {
                switch (feedType)
                {
                case FEED_TBT:
                    return FEED_TBT_ON;
                case FEED_SNAPSHOT:
                    return FEED_SNAPSHOT_ON;
                default:
                    return FEED_SNAPSHOT_ON;
                }
            }
            // getting feedType from the bits
            inline __attribute__((always_inline)) unsigned char getFeedTypeFromBits(uint8_t feedTypeBits)
            {
                switch (feedTypeBits)
                {
                case FEED_TBT_ON:
                    return FEED_TBT;
                case FEED_SNAPSHOT_ON:
                    return FEED_SNAPSHOT;
                default:
                    return FEED_SNAPSHOT;
                }
            }

            inline __attribute__((always_inline))
            uint32_t
            convertUpdateTypeToBits(unsigned char updateType)
            {
                switch (updateType)
                {
                case MDUPDTYPE_ADD:
                    return MDUPDTYPE_ADD_ON;
                case MDUPDTYPE_MODIFY:
                    return MDUPDTYPE_MODIFY_ON;
                case MDUPDTYPE_DELETE:
                    return MDUPDTYPE_DELETE_ON;
                case MDUPDTYPE_ORDER_ENTRY:
                    return MDUPDTYPE_ORDER_ENTRY_ON;
                case MDUPDTYPE_TRADE:
                    return MDUPDTYPE_TRADE_ON;
                case MDUPDTYPE_TRADE_INFO:
                    return MDUPDTYPE_TRADE_INFO_ON;
                case MDUPDTYPE_TRADE_IMPLIED:
                    return MDUPDTYPE_TRADE_IMPLIED_ON;
                case MDUPDTYPE_NONE:
                    return MDUPDTYPE_NONE_ON;
                default:
                    return MDUPDTYPE_NONE_ON;
                }
            }

            inline __attribute__((always_inline)) unsigned char getUpdateTypeFromBits(uint32_t updateTypeBits)
            {
                switch (updateTypeBits)
                {
                case MDUPDTYPE_ADD_ON:
                    return MDUPDTYPE_ADD;
                case MDUPDTYPE_MODIFY_ON:
                    return MDUPDTYPE_MODIFY;
                case MDUPDTYPE_DELETE_ON:
                    return MDUPDTYPE_DELETE;
                case MDUPDTYPE_ORDER_ENTRY_ON:
                    return MDUPDTYPE_ORDER_ENTRY;
                case MDUPDTYPE_TRADE_ON:
                    return MDUPDTYPE_TRADE;
                case MDUPDTYPE_TRADE_INFO_ON:
                    return MDUPDTYPE_TRADE_INFO;
                case MDUPDTYPE_TRADE_IMPLIED_ON:
                    return MDUPDTYPE_TRADE_IMPLIED;
                case MDUPDTYPE_NONE_ON:
                    return MDUPDTYPE_NONE;
                default:
                    return MDUPDTYPE_NONE;
                }
            }

            inline __attribute__((always_inline))
            uint32_t
            convertEndPktToBits(uint8_t endPkt)
            {
                return endPkt;
            }

            inline __attribute__((always_inline))
            uint8_t
            getEndPktFromBits(uint32_t endPktBits)
            {
                return endPktBits;
            }
        };

        struct SerializedMdUpdateON
        {
            char buffer[sizeof(MarketUpdateOverNet)];
            uint16_t size;
        };
    }
}

#pragma pack(pop)
#endif /* MARKETUPDATEONNETWORK_H_ */

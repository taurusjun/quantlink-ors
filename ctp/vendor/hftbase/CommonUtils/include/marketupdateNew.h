/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 * Note: marketupdateonnetwork.h is dependent on this file
 */
#ifndef _MARKET_UPDATE_NEW_H
#define _MARKET_UPDATE_NEW_H

#include <inttypes.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace illuminati
{

    namespace md
    {
        const int32_t INTEREST_LEVELS = 20;
        const int32_t MAX_SYMBOL_SIZE = 50;

        /* m_exchangeName code */
        const unsigned char EXCHANGE_UNKNOWN = 0;
        const unsigned char NSE_FO = 1;
        const unsigned char NSE_CM = 2;
        const unsigned char MICEX_FOND = 3;
        const unsigned char MICEX_CURR = 4;
        const unsigned char MICEX_FO = 5;
        const unsigned char FORTS_F = 6; // Depricated
        const unsigned char FORTS_O = 7; // Depricated
        const unsigned char MCX_FUTCOM = 8;
        const unsigned char NSE_CDS = 9;
        const unsigned char BSE = 10;
        const unsigned char MCX_SX = 11;
        const unsigned char REUTERS_GLOBAL = 12;
        const unsigned char LSE_IOB = 13;
        const unsigned char BRAZIL = 14;
        const unsigned char FLEXTRADE = 15;
        const unsigned char DGCX = 16;
        const unsigned char RDM = 17;
        const unsigned char PUMA_FXFU = 18;
        const unsigned char PUMA_FXEQOPT = 19;
        const unsigned char PUMA_IRFU = 20;
        const unsigned char PUMA_IROPT = 21;
        const unsigned char PUMA_CMFU = 22;
        const unsigned char PUMA_CMOPT = 23;
        const unsigned char PUMA_EQFU = 24;
        const unsigned char PUMA_SPFU = 25;
        const unsigned char PUMA_FXSPOT = 26;
        const unsigned char PUMA_EQIX = 27;
        const unsigned char PLAZA_FUT = 28;        // Russia FORTS Futures, Plaza2 feed
        const unsigned char KOSPI_FNO = 29;        // KOSPI200 INDEX FUTURES and OPTIONS (CALL + PUT)
        const unsigned char KOSPI_EQUITY = 30;     // NOT used currently, reserved for KOSPI EQUITIES
        const unsigned char PUMA_EQ_MAT = 31;      // Mining, Steel, Metallurgy and Basic Materials
        const unsigned char PUMA_EQ_OPT_MAT = 32;  // Options from Mining, Steel, Metallurgy and Basic Materials
        const unsigned char PUMA_EQ_OIL = 33;      // Oil and Bio-fuels
        const unsigned char PUMA_EQ_OPT_OIL = 34;  // Options Oil and Bio-fuels
        const unsigned char PUMA_EQ_FIN = 35;      // Financial and Others, Holding, Financial Intermediaries
        const unsigned char PUMA_EQ_OPT_FIN = 36;  // Options from Financial and Others, Holding, Financial Intermediaries
        const unsigned char PUMA_EQ_GD = 37;       // Industrial Goods, Construction, Consumption, Transport and Other Sectors
        const unsigned char PUMA_EQ_OPT_GD = 38;   // Options from Industrial Goods, Construction, Consumption, Transport and Other Sectors
        const unsigned char PUMA_EQ_OTC = 39;      // OTC
        const unsigned char PUMA_EQ_OPT_IBOV = 40; // Options from IBOVESPA
        const unsigned char PUMA_EQ = 41;          // Brazil Cash Market
        const unsigned char PUMA_FU = 42;          // Brazil Futures
        const unsigned char NYSE = 43;             // Lime NYSE feed
        const unsigned char ARCA = 44;             // Lime ARCA feed
        const unsigned char PACF = 45;
        const unsigned char NQEX = 46;
        const unsigned char NQNM = 47;
        const unsigned char NASDAQ = 48;
        const unsigned char IB_GLOBAL = 49;
        const unsigned char CHINA = 50;
        const unsigned char BATS = 51; // Lime BATS feed
        const unsigned char INET = 52; // Lime INET feed
        const unsigned char CBSX = 53; // Lime CBSX feed
        const unsigned char CME = 54;
        const unsigned char LME = 55;
        const unsigned char FASTMD = 56; // USINFRA
        const unsigned char CHINA_SHFE = 57;
        const unsigned char CHINA_CFFEX = 58;
        const unsigned char CHINA_ZCE = 59;
        const unsigned char CHINA_DCE = 60;
        const unsigned char CHINA_GFEX = 61;
        const unsigned char SGX = 62; // Singapore
        const unsigned char OSE = 63; // Osaka
        const unsigned char MCASTNSE_FO = 64;
        const unsigned char MCASTNSE_CDS = 65;
        const unsigned char MCASTNSE_CM = 66;
        const unsigned char ESPEED_TB = 67;
        const unsigned char ICEUS_BRN = 68;
        const unsigned char KOSPI_CUR = 69; // KOSPI Currency USDKRW
        const unsigned char CHINA_SH = 70;  // ERICEDIT 0803
        const unsigned char CHINA_SZ = 71;
        const unsigned char CHINA_INE = 72;
        // Update this whenever you add a new exchange
        const size_t MAX_EXCHANGE_COUNT = 73;

        const unsigned char REQUEST_MSG = 100;
        const unsigned char RESPONSE_MSG = 101;

        /* m_feedType */
        const unsigned char FEED_TBT = 'X';
        const unsigned char FEED_SNAPSHOT = 'W';
        const unsigned char FEED_AUCTION = 'A';

        /* m_side */
        const unsigned char SIDE_BUY = 'B';
        const unsigned char SIDE_SELL = 'S';
        const unsigned char SIDE_SHORT_SELL = 'A';
        const unsigned char SIDE_NONE = 'N'; // Can be used for Snapshot

        /* m_updateType */
        const unsigned char MDUPDTYPE_ADD = 'A';             // new update
        const unsigned char MDUPDTYPE_MODIFY = 'M';          // modify update
        const unsigned char MDUPDTYPE_DELETE = 'D';          // delete/cancel update
        const unsigned char MDUPDTYPE_INTERNAL_DELETE = 'V'; // internal generated delete/cancel update used for CME
        const unsigned char MDUPDTYPE_ORDER_ENTRY = 'O';     // can be used in snapshot
        const unsigned char MDUPDTYPE_TRADE = 'X';           // Exchange sent Trade plus an orderbook update
        const unsigned char MDUPDTYPE_TRADE_INFO = 'I';      // Exchange sent Trade Info
        const unsigned char MDUPDTYPE_TRADE_IMPLIED = 'T';   // Predicted Trade plus an order book update
        const unsigned char MDUPDTYPE_NONE = 'N';            // Undefined m_updateType
        const unsigned char MDUPDTYPE_IMBALANCE = 'B';
        const unsigned char MDUPDTYPE_MOD_ERR = 'J';
        const unsigned char MDUPDTYPE_BOOKRESET = 'R';  // added for CME
        const unsigned char MDUPDTYPE_DELETEFROM = 'e'; // to be implemented
        const unsigned char MDUPDTYPE_DELETETHRU = 'f'; // to be implemented
        const unsigned char MDUPDTYPE_OVERLAY = 'g';    // to be implemented

        /* m_updateLevel */
        const int8_t MDUPDLEVEL_NONE = -1;

        struct orderQtPair_t
        {
            int32_t quantity;
            int32_t orderCount;
        };

        struct bookElement_t : public orderQtPair_t
        {
            double price;

            std::string toString() const
            {
                std::ostringstream ss;
                ss << std::setw(12) << std::setprecision(10) << price
                   << std::setw(10) << quantity
                   << std::setw(10) << orderCount;

                return ss.str();
            }

            void print() const
            {
                std::cout << toString();
            }
        };

        const int32_t BOOKELEMENT_SIZE = sizeof(bookElement_t);

        /*------------------------------------------------------------------------------------
         * The MarketUpdateNew structure
         * If you make changes here, DataHeaderNew in marketdeltaNew.h may also require changes
         *-----------------------------------------------------------------------------------*/
        struct MDHeaderPart
        {
            uint64_t m_exchTS;
            uint64_t m_timestamp;
            uint64_t m_seqnum;
            uint64_t m_rptseqnum;
            uint64_t m_tokenId;
            // not changing MAX_SYMBOL_SIZE because it is used all over the place, and I do not want the size of some structure somwhere to change, in case the size matters.
            char m_symbol[MAX_SYMBOL_SIZE - 2];
            uint16_t m_symbolID;
            unsigned char m_exchangeName;

            bool equals(const MDHeaderPart &other) const
            {
                bool retval = true;
                if (m_exchTS != other.m_exchTS)
                    retval = false;
                if (m_timestamp != other.m_timestamp)
                    retval = false;
                if (m_seqnum != other.m_seqnum)
                    retval = false;
                if (m_rptseqnum != other.m_rptseqnum)
                    retval = false;
                if (m_tokenId != other.m_tokenId)
                    retval = false;
                if (strcmp(m_symbol, other.m_symbol))
                    retval = false;
                if (m_exchangeName != other.m_exchangeName)
                    retval = false;

                return retval;
            }

            void printLine(void) const
            {
                std::cout
                    << "|Symbol=" << m_symbol
                    << "|ExchangeTS=" << m_exchTS
                    << "|LocalTS=" << m_timestamp
                    << "|Seq=" << m_seqnum
                    << "|RptSeqnum=" << m_rptseqnum
                    << "|TokenId=" << m_tokenId
                    << "|ExchNameCode" << (int)m_exchangeName;
            }

            std::string toString(void) const
            {
                std::ostringstream ss;
                ss
                    << "|Symbol=" << m_symbol
                    << "|ExchangeTS=" << m_exchTS
                    << "|LocalTS=" << m_timestamp
                    << "|Seq=" << m_seqnum
                    << "|RptSeqnum=" << m_rptseqnum
                    << "|TokenId=" << m_tokenId
                    << "|ExchNameCode=" << (int)m_exchangeName;
                return ss.str();
            }

            void print(void) const
            {
                std::cout << toString();
            }
        };

        struct MDDataPart
        {
            double m_newPrice;
            double m_oldPrice;
            double m_lastTradedPrice;
            uint64_t m_lastTradedTime;
            double m_totalTradedValue;
            int64_t m_totalTradedQuantity;
            double m_yield;
            bookElement_t m_bidUpdates[INTEREST_LEVELS]; /* 320 */
            bookElement_t m_askUpdates[INTEREST_LEVELS]; /* 320 */
            int32_t m_newQuant;
            int32_t m_oldQuant;
            int32_t m_lastTradedQuantity;
            int8_t m_validBids;
            int8_t m_validAsks;
            int8_t m_updateLevel;
            uint8_t m_endPkt;
            unsigned char m_side;
            unsigned char m_updateType;
            unsigned char m_feedType;

            void printLine(unsigned char m_exchangeName = illuminati::md::EXCHANGE_UNKNOWN) const
            {
                std::cout
                    << "|updateType=" << m_updateType
                    << "|newPrice=" << std::setprecision(10) << m_newPrice
                    << "|oldPrice=" << std::setprecision(10) << m_oldPrice
                    << "|newQuantity=" << m_newQuant
                    << "|oldQuantity=" << m_oldQuant
                    << "|LTP=" << std::setprecision(10) << m_lastTradedPrice
                    << "|LTQ=" << m_lastTradedQuantity
                    << "|LTT=" << m_lastTradedTime
                    << "|TTV=" << std::setprecision(15) << m_totalTradedValue
                    << "|TTQ=" << m_totalTradedQuantity
                    << "|validBids=" << (int)m_validBids
                    << "|validAsks=" << (int)m_validAsks
                    << "|updateLevel=" << (int)m_updateLevel
                    << "|endPacket=" << (int)m_endPkt
                    << "|side=" << m_side
                    << "|feedType=" << m_feedType;

                if (m_exchangeName == illuminati::md::MCASTNSE_FO || m_exchangeName == illuminati::md::MCASTNSE_CM || m_exchangeName == illuminati::md::MCASTNSE_CDS)
                {
                    std::cout << "|Exchange_Order_Id=" << std::setprecision(30) << m_yield;
                }
                else
                {
                    std::cout << "|Yield=" << std::setprecision(30) << m_yield;
                }

                std::cout << "\nAsk:";
                for (int i = 0; i < m_validAsks; i++)
                    std::cout << "|P=" << std::setprecision(10) << m_askUpdates[i].price << "|Q=" << m_askUpdates[i].quantity << "|C=" << m_askUpdates[i].orderCount;

                std::cout << "\nBid:";
                for (int i = 0; i < m_validBids; i++)
                    std::cout << "|P=" << std::setprecision(10) << m_bidUpdates[i].price << "|Q=" << m_bidUpdates[i].quantity << "|C=" << m_bidUpdates[i].orderCount;

                std::cout << "\n";
            }

            std::string toString(bool checkBidAskOrder = false, unsigned char m_exchangeName = illuminati::md::EXCHANGE_UNKNOWN) const
            {
                std::ostringstream ss;
                ss
                    << "|updateType=" << m_updateType
                    << "|newPrice=" << std::setprecision(10) << m_newPrice
                    << "|oldPrice=" << std::setprecision(10) << m_oldPrice
                    << "|newQuantity=" << m_newQuant
                    << "|oldQuantity=" << m_oldQuant
                    << "|LTP=" << std::setprecision(10) << m_lastTradedPrice
                    << "|LTQ=" << m_lastTradedQuantity
                    << "|LTT=" << m_lastTradedTime
                    << "|TTV=" << std::setprecision(15) << m_totalTradedValue
                    << "|TTQ=" << m_totalTradedQuantity
                    << "|validBids=" << (int)m_validBids
                    << "|validAsks=" << (int)m_validAsks
                    << "|updateLevel=" << (int)m_updateLevel
                    << "|endPacket=" << (int)m_endPkt
                    << "|side=" << m_side
                    << "|feedType=" << m_feedType;
                if (m_exchangeName == illuminati::md::MCASTNSE_FO || m_exchangeName == illuminati::md::MCASTNSE_CM || m_exchangeName == illuminati::md::MCASTNSE_CDS)
                {
                    ss << "|Exchange_Order_Id=" << std::setprecision(30) << m_yield;
                }
                else
                {
                    ss << "|Yield=" << std::setprecision(30) << m_yield;
                }

                ss << "\n";
                int32_t pxw = 18, qw = 10, ow = 10;
                ss << std::setw(ow) << "Bid[#]" << std::setw(qw) << "Qty" << std::setw(pxw) << "Price" << " | ";
                ss << std::setiosflags(std::ios::left);
                ss << std::setw(pxw) << "Price";
                ss << std::resetiosflags(std::ios::left);
                ss << std::setw(qw) << "Qty" << std::setw(ow) << "Ask[#]" << "   Idx";
                ss << "\n";
                ss << std::setw(ow) << "------" << std::setw(qw) << "---" << std::setw(pxw) << "-----" << " | ";
                ss << std::setiosflags(std::ios::left);
                ss << std::setw(pxw) << "-----";
                ss << std::resetiosflags(std::ios::left);
                ss << std::setw(qw) << "---" << std::setw(ow) << "------" << "   ---";
                ss << "\n";

                int32_t askCount = 0, bidCount = 0;
                bool printBid = false, printAsk = false;
                int index = 0;
                std::string err("");
                while (true)
                {
                    printBid = (bidCount < m_validBids) ? true : false;
                    printAsk = (askCount < m_validAsks) ? true : false;

                    if (!printAsk && !printBid)
                        break;

                    if (checkBidAskOrder)
                    {
                        err = "";
                        if (printAsk && printBid && (0 == index) && (m_bidUpdates[0].price >= m_askUpdates[0].price) && m_updateType != MDUPDTYPE_TRADE_INFO)
                            err = " <*Cross";
                    }

                    if (printBid)
                    {
                        ss << std::setw(ow) << m_bidUpdates[bidCount].orderCount
                           << std::setw(qw) << m_bidUpdates[bidCount].quantity
                           << std::setw(pxw) << std::setprecision(10) << m_bidUpdates[bidCount].price;
                        if (checkBidAskOrder && bidCount > 0 && (m_bidUpdates[bidCount].price >= m_bidUpdates[bidCount - 1].price))
                            err = " <*Bid-E";
                    }
                    else
                    {
                        ss << std::setw(ow) << 0
                           << std::setw(qw) << 0
                           << std::setw(pxw) << std::setprecision(pxw) << 0;
                    }
                    ss << " | ";
                    bidCount++;

                    if (printAsk)
                    {
                        ss << std::setiosflags(std::ios::left);
                        ss << std::setw(pxw) << std::setprecision(10) << m_askUpdates[askCount].price;
                        ss << std::resetiosflags(std::ios::left);
                        ss << std::setw(qw) << m_askUpdates[askCount].quantity
                           << std::setw(ow) << m_askUpdates[askCount].orderCount;
                        if (checkBidAskOrder && askCount > 0 && (m_askUpdates[askCount].price <= m_askUpdates[askCount - 1].price))
                            err = " <*Ask-E";
                    }
                    else
                    {
                        ss << std::setiosflags(std::ios::left);
                        ss << std::setiosflags(std::ios::left) << std::setw(pxw) << std::setprecision(10) << 0;
                        ss << std::resetiosflags(std::ios::left);
                        ss << std::setw(qw) << 0
                           << std::setw(ow) << 0;
                    }

                    if (checkBidAskOrder)
                        ss << " | " << index << err;
                    else
                        ss << " | " << index;

                    askCount++;

                    if (index == m_updateLevel)
                    {
                        ss << " <" << m_side << ":" << m_updateType;
                    }
                    ++index;

                    ss << "\n";
                }
                return ss.str();
            }

            void print(unsigned char m_exchangeName = illuminati::md::EXCHANGE_UNKNOWN) const
            {
                std::cout << toString();
            }

            // Helper for debug, and info
            bool equals(const MDDataPart &other) const
            {
                bool retval = true;
                if (m_newPrice != other.m_newPrice)
                    retval = false;
                if (m_oldPrice != other.m_oldPrice)
                    retval = false;
                if (m_newQuant != other.m_newQuant)
                    retval = false;
                if (m_oldQuant != other.m_oldQuant)
                    retval = false;
                if (m_lastTradedPrice != other.m_lastTradedPrice)
                    retval = false;
                if (m_lastTradedTime != other.m_lastTradedTime)
                    retval = false;
                if (m_totalTradedValue != other.m_totalTradedValue)
                    retval = false;
                if (m_totalTradedQuantity != other.m_totalTradedQuantity)
                    retval = false;
                if (m_lastTradedQuantity != other.m_lastTradedQuantity)
                    retval = false;
                if (m_yield != other.m_yield)
                    retval = false;
                if (m_validBids != other.m_validBids)
                    retval = false;
                if (m_validAsks != other.m_validAsks)
                    retval = false;
                if (m_updateLevel != other.m_updateLevel)
                    retval = false;
                if (m_endPkt != other.m_endPkt)
                    retval = false;
                if (m_side != other.m_side)
                    retval = false;
                if (m_updateType != other.m_updateType)
                    retval = false;
                if (m_feedType != other.m_feedType)
                    retval = false;

                for (int i = 0; i < m_validAsks; i++)
                {
                    if ((m_askUpdates[i].price != other.m_askUpdates[i].price) ||
                        (m_askUpdates[i].quantity != other.m_askUpdates[i].quantity) ||
                        (m_askUpdates[i].orderCount != other.m_askUpdates[i].orderCount))
                    {
                        retval = false;
                    }
                }

                for (int i = 0; i < m_validBids; i++)
                {
                    if ((m_bidUpdates[i].price != other.m_bidUpdates[i].price) ||
                        (m_bidUpdates[i].quantity != other.m_bidUpdates[i].quantity) ||
                        (m_bidUpdates[i].orderCount != other.m_bidUpdates[i].orderCount))
                        retval = false;
                }

                return retval;
            }
        };

        struct MarketUpdateNew : public MDHeaderPart, MDDataPart
        {
            void print(void) const
            {
                MDHeaderPart::print();
                MDDataPart::print(m_exchangeName);
            }

            void printLine(void) const
            {
                MDHeaderPart::printLine();
                MDDataPart::printLine(m_exchangeName);
            }

            std::string toString(bool checkBidAskOrder = false) const
            {
                std::ostringstream ss;
                ss << MDHeaderPart::toString() << MDDataPart::toString(checkBidAskOrder, m_exchangeName) << "\n";
                return ss.str();
            }

            bool operator==(const MarketUpdateNew &md)
            {
                return MDDataPart::equals((MDDataPart)md) && MDHeaderPart::equals((MDHeaderPart)md);
            }

            /*!
             * Method to find first greater or equal element
             */
            bookElement_t *firstLessOrEqualBidBook(double value)
            {
                bookElement_t *first = m_bidUpdates;
                bookElement_t *it = NULL;
                int count = m_validBids;
                int step = 0;
                while (count > 0)
                {
                    it = first;
                    step = count / 2;
                    it += step; // it points to the mid

                    // std::cout << "it=" << *it << " count=" << count << " step=" << step << " first=" << *first << std::endl;

                    if (it->price > value)
                    {
                        first = ++it; // first is moved to the other half of the list where there are greater elements
                        count -= step + 1;
                    }
                    else
                        count = step;
                }

                return first;
            }

            bookElement_t *firstGreaterOrEqualAskBook(double value)
            {
                bookElement_t *first = m_askUpdates;
                bookElement_t *it = NULL;
                int count = m_validAsks;
                int step = 0;
                while (count > 0)
                {
                    it = first;
                    step = count / 2;
                    it += step; // it points to the mid

                    // std::cout << "it=" << *it << " count=" << count << " step=" << step << " first=" << *first << std::endl;

                    if (it->price < value)
                    {
                        first = ++it; // first is moved to the other half of the list where there are greater elements
                        count -= step + 1;
                    }
                    else
                        count = step;
                }

                return first;
            }
        };

        template <int InterestLevels>
        struct MDDataPart_t
        {
            double m_newPrice;
            double m_oldPrice;
            double m_lastTradedPrice;
            uint64_t m_lastTradedTime;
            double m_totalTradedValue;
            int64_t m_totalTradedQuantity;
            double m_yield;
            bookElement_t m_bidUpdates[InterestLevels];
            bookElement_t m_askUpdates[InterestLevels];
            int32_t m_newQuant;
            int32_t m_oldQuant;
            int32_t m_lastTradedQuantity;
            int8_t m_validBids;
            int8_t m_validAsks;
            int8_t m_updateLevel;
            bool m_endPkt;
            unsigned char m_side;
            unsigned char m_updateType;
            unsigned char m_feedType;

            void printLine(unsigned char m_exchangeName = illuminati::md::EXCHANGE_UNKNOWN) const
            {
                std::cout
                    << "|updateType=" << m_updateType
                    << "|newPrice=" << std::setprecision(10) << m_newPrice
                    << "|oldPrice=" << std::setprecision(10) << m_oldPrice
                    << "|newQuantity=" << m_newQuant
                    << "|oldQuantity=" << m_oldQuant
                    << "|LTP=" << std::setprecision(10) << m_lastTradedPrice
                    << "|LTQ=" << m_lastTradedQuantity
                    << "|LTT=" << m_lastTradedTime
                    << "|TTV=" << std::setprecision(15) << m_totalTradedValue
                    << "|TTQ=" << m_totalTradedQuantity
                    << "|validBids=" << (int)m_validBids
                    << "|validAsks=" << (int)m_validAsks
                    << "|updateLevel=" << (int)m_updateLevel
                    << "|endPacket=" << (int)m_endPkt
                    << "|side=" << m_side
                    << "|feedType=" << m_feedType;
                if (m_exchangeName == illuminati::md::MCASTNSE_FO || m_exchangeName == illuminati::md::MCASTNSE_CM || m_exchangeName == illuminati::md::MCASTNSE_CDS)
                {
                    std::cout << "|Exchange_Order_Id=" << std::setprecision(30) << m_yield;
                }
                else
                {
                    std::cout << "|Yield=" << std::setprecision(30) << m_yield;
                }

                std::cout << "\nAsk:";
                for (int i = 0; i < m_validAsks; i++)
                    std::cout << "|P=" << std::setprecision(10) << m_askUpdates[i].price << "|Q=" << m_askUpdates[i].quantity << "|C=" << m_askUpdates[i].orderCount;

                std::cout << "\nBid:";
                for (int i = 0; i < m_validBids; i++)
                    std::cout << "|P=" << std::setprecision(10) << m_bidUpdates[i].price << "|Q=" << m_bidUpdates[i].quantity << "|C=" << m_bidUpdates[i].orderCount;

                std::cout << "\n";
            }

            std::string toString(unsigned char m_exchangeName = illuminati::md::EXCHANGE_UNKNOWN) const
            {
                std::ostringstream ss;
                ss
                    << "|updateType=" << m_updateType
                    << "|newPrice=" << std::setprecision(10) << m_newPrice
                    << "|oldPrice=" << std::setprecision(10) << m_oldPrice
                    << "|newQuantity=" << m_newQuant
                    << "|oldQuantity=" << m_oldQuant
                    << "|LTP=" << std::setprecision(10) << m_lastTradedPrice
                    << "|LTQ=" << m_lastTradedQuantity
                    << "|LTT=" << m_lastTradedTime
                    << "|TTV=" << std::setprecision(15) << m_totalTradedValue
                    << "|TTQ=" << m_totalTradedQuantity
                    << "|validBids=" << (int)m_validBids
                    << "|validAsks=" << (int)m_validAsks
                    << "|updateLevel=" << (int)m_updateLevel
                    << "|endPacket=" << (int)m_endPkt
                    << "|side=" << m_side
                    << "|feedType=" << m_feedType;

                if (m_exchangeName == illuminati::md::MCASTNSE_FO || m_exchangeName == illuminati::md::MCASTNSE_CM || m_exchangeName == illuminati::md::MCASTNSE_CDS)
                {
                    ss << "|Exchange_Order_Id=" << std::setprecision(30) << m_yield;
                }
                else
                {
                    ss << "|Yield=" << std::setprecision(30) << m_yield;
                }

                ss << "\n";
                int32_t pxw = 18, qw = 10, ow = 10;
                ss << std::setw(ow) << "Bid[#]" << std::setw(qw) << "Qty" << std::setw(pxw) << "Price" << " | ";
                ss << std::setiosflags(std::ios::left);
                ss << std::setw(pxw) << "Price";
                ss << std::resetiosflags(std::ios::left);
                ss << std::setw(qw) << "Qty" << std::setw(ow) << "Ask[#]" << "   Idx";
                ss << "\n";
                ss << std::setw(ow) << "------" << std::setw(qw) << "---" << std::setw(pxw) << "-----" << " | ";
                ss << std::setiosflags(std::ios::left);
                ss << std::setw(pxw) << "-----";
                ss << std::resetiosflags(std::ios::left);
                ss << std::setw(qw) << "---" << std::setw(ow) << "------" << "   ---";
                ss << "\n";

                int32_t askCount = 0, bidCount = 0;
                bool printBid = false, printAsk = false;
                int index = 0;
                while (true)
                {

                    printBid = (bidCount < m_validBids) ? true : false;
                    printAsk = (askCount < m_validAsks) ? true : false;

                    if (!printAsk && !printBid)
                        break;

                    if (printBid)
                    {
                        ss << std::setw(ow) << m_bidUpdates[bidCount].orderCount
                           << std::setw(qw) << m_bidUpdates[bidCount].quantity
                           << std::setw(pxw) << std::setprecision(10) << m_bidUpdates[bidCount].price;
                    }
                    else
                    {
                        ss << std::setw(ow) << 0
                           << std::setw(qw) << 0
                           << std::setw(pxw) << std::setprecision(pxw) << 0;
                    }
                    ss << " | ";
                    bidCount++;
                    if (printAsk)
                    {
                        ss << std::setiosflags(std::ios::left);
                        ss << std::setw(pxw) << std::setprecision(10) << m_askUpdates[askCount].price;
                        ss << std::resetiosflags(std::ios::left);
                        ss << std::setw(qw) << m_askUpdates[askCount].quantity
                           << std::setw(ow) << m_askUpdates[askCount].orderCount;
                    }
                    else
                    {
                        ss << std::setiosflags(std::ios::left);
                        ss << std::setiosflags(std::ios::left) << std::setw(pxw) << std::setprecision(10) << 0;
                        ss << std::resetiosflags(std::ios::left);
                        ss << std::setw(qw) << 0
                           << std::setw(ow) << 0;
                    }
                    ss << " | " << index;
                    if (index == m_updateLevel)
                        ss << " <" << m_side << ":" << m_updateType;
                    ++index;

                    askCount++;

                    ss << "\n";
                }
                return ss.str();
            }

            void print(void) const
            {
                std::cout << toString();
            }

            // Helper for debug, and info
            bool equals(const MDDataPart_t &other) const
            {
                bool retval = true;
                if (m_newPrice != other.m_newPrice)
                    retval = false;
                if (m_oldPrice != other.m_oldPrice)
                    retval = false;
                if (m_newQuant != other.m_newQuant)
                    retval = false;
                if (m_oldQuant != other.m_oldQuant)
                    retval = false;
                if (m_lastTradedPrice != other.m_lastTradedPrice)
                    retval = false;
                if (m_lastTradedTime != other.m_lastTradedTime)
                    retval = false;
                if (m_totalTradedValue != other.m_totalTradedValue)
                    retval = false;
                if (m_totalTradedQuantity != other.m_totalTradedQuantity)
                    retval = false;
                if (m_lastTradedQuantity != other.m_lastTradedQuantity)
                    retval = false;
                if (m_yield != other.m_yield)
                    retval = false;
                if (m_validBids != other.m_validBids)
                    retval = false;
                if (m_validAsks != other.m_validAsks)
                    retval = false;
                if (m_updateLevel != other.m_updateLevel)
                    retval = false;
                if (m_endPkt != other.m_endPkt)
                    retval = false;
                if (m_side != other.m_side)
                    retval = false;
                if (m_updateType != other.m_updateType)
                    retval = false;
                if (m_feedType != other.m_feedType)
                    retval = false;

                for (int i = 0; i < m_validAsks; i++)
                {
                    if ((m_askUpdates[i].price != other.m_askUpdates[i].price) ||
                        (m_askUpdates[i].quantity != other.m_askUpdates[i].quantity) ||
                        (m_askUpdates[i].orderCount != other.m_askUpdates[i].orderCount))
                    {
                        retval = false;
                    }
                }

                for (int i = 0; i < m_validBids; i++)
                {
                    if ((m_bidUpdates[i].price != other.m_bidUpdates[i].price) ||
                        (m_bidUpdates[i].quantity != other.m_bidUpdates[i].quantity) ||
                        (m_bidUpdates[i].orderCount != other.m_bidUpdates[i].orderCount))
                        retval = false;
                }

                return retval;
            }
        };

        template <int InterestLevels>
        struct TMarketUpdateNew : public MDHeaderPart, MDDataPart_t<InterestLevels>
        {
            void print(void) const
            {
                MDHeaderPart::print();
                MDDataPart_t<InterestLevels>::print();
            }

            void printLine(void) const
            {
                MDHeaderPart::printLine();
                MDDataPart_t<InterestLevels>::printLine();
            }

            void copyFrom(const MarketUpdateNew &update)
            {
                // header
                m_exchTS = update.m_exchTS;
                m_timestamp = update.m_timestamp;
                m_seqnum = update.m_seqnum;
                m_rptseqnum = update.m_rptseqnum;
                m_tokenId = update.m_tokenId;
                memcpy(m_symbol, update.m_symbol, MAX_SYMBOL_SIZE);
                m_exchangeName = update.m_exchangeName;

                // DATA
                MDDataPart_t<InterestLevels>::m_newPrice = update.m_newPrice;
                MDDataPart_t<InterestLevels>::m_oldPrice = update.m_oldPrice;
                MDDataPart_t<InterestLevels>::m_lastTradedPrice = update.m_lastTradedPrice;
                MDDataPart_t<InterestLevels>::m_lastTradedTime = update.m_lastTradedTime;
                MDDataPart_t<InterestLevels>::m_totalTradedValue = update.m_totalTradedValue;
                MDDataPart_t<InterestLevels>::m_totalTradedQuantity = update.m_totalTradedQuantity;
                MDDataPart_t<InterestLevels>::m_yield = update.m_yield;
                MDDataPart_t<InterestLevels>::m_newQuant = update.m_newQuant;
                MDDataPart_t<InterestLevels>::m_oldQuant = update.m_oldQuant;
                MDDataPart_t<InterestLevels>::m_lastTradedQuantity = update.m_lastTradedQuantity;
                MDDataPart_t<InterestLevels>::m_validBids = update.m_validBids;
                MDDataPart_t<InterestLevels>::m_validAsks = update.m_validAsks;
                MDDataPart_t<InterestLevels>::m_updateLevel = update.m_updateLevel;
                MDDataPart_t<InterestLevels>::m_endPkt = update.m_endPkt;
                MDDataPart_t<InterestLevels>::m_side = update.m_side;
                MDDataPart_t<InterestLevels>::m_updateType = update.m_updateType;
                MDDataPart_t<InterestLevels>::m_feedType = update.m_feedType;

                memcpy((void *)MDDataPart_t<InterestLevels>::m_bidUpdates,
                       &(update.m_bidUpdates), InterestLevels * sizeof(bookElement_t));
                memcpy((void *)MDDataPart_t<InterestLevels>::m_askUpdates,
                       &(update.m_askUpdates), InterestLevels * sizeof(bookElement_t));
            }

            void copyOver(MarketUpdateNew &update) const
            {
                // header
                update.m_exchTS = m_exchTS;
                update.m_timestamp = m_timestamp;
                update.m_seqnum = m_seqnum;
                update.m_rptseqnum = m_rptseqnum;
                update.m_tokenId = m_tokenId;
                memcpy(update.m_symbol, m_symbol, MAX_SYMBOL_SIZE);
                update.m_exchangeName = m_exchangeName;

                // DATA
                update.m_newPrice = MDDataPart_t<InterestLevels>::m_newPrice;
                update.m_oldPrice = MDDataPart_t<InterestLevels>::m_oldPrice;
                update.m_lastTradedPrice = MDDataPart_t<InterestLevels>::m_lastTradedPrice;
                update.m_lastTradedTime = MDDataPart_t<InterestLevels>::m_lastTradedTime;
                update.m_totalTradedValue = MDDataPart_t<InterestLevels>::m_totalTradedValue;
                update.m_totalTradedQuantity = MDDataPart_t<InterestLevels>::m_totalTradedQuantity;
                update.m_yield = MDDataPart_t<InterestLevels>::m_yield;
                update.m_newQuant = MDDataPart_t<InterestLevels>::m_newQuant;
                update.m_oldQuant = MDDataPart_t<InterestLevels>::m_oldQuant;
                update.m_lastTradedQuantity = MDDataPart_t<InterestLevels>::m_lastTradedQuantity;
                update.m_validBids = MDDataPart_t<InterestLevels>::m_validBids;
                update.m_validAsks = MDDataPart_t<InterestLevels>::m_validAsks;
                update.m_updateLevel = MDDataPart_t<InterestLevels>::m_updateLevel;
                update.m_endPkt = MDDataPart_t<InterestLevels>::m_endPkt;
                update.m_side = MDDataPart_t<InterestLevels>::m_side;
                update.m_updateType = MDDataPart_t<InterestLevels>::m_updateType;
                update.m_feedType = MDDataPart_t<InterestLevels>::m_feedType;

                memcpy((void *)&(update.m_bidUpdates), MDDataPart_t<InterestLevels>::m_bidUpdates,
                       InterestLevels * sizeof(bookElement_t));
                memcpy((void *)&(update.m_askUpdates), MDDataPart_t<InterestLevels>::m_askUpdates,
                       InterestLevels * sizeof(bookElement_t));
            }

            std::string toString(void) const
            {
                std::ostringstream ss;
                ss << MDHeaderPart::toString() << MDDataPart_t<InterestLevels>::toString() << "\n";
                return ss.str();
            }
        };

        static char getExchangeIdFromName(const std::string &token)
        {
            if (token == "EXCHANGE_UNKNOWN")
                return illuminati::md::EXCHANGE_UNKNOWN;
            if (token == "NSE_FO")
                return illuminati::md::NSE_FO;
            if (token == "NSE_CM")
                return illuminati::md::NSE_CM;
            if (token == "MICEX_FOND")
                return illuminati::md::MICEX_FOND;
            if (token == "MICEX_CURR")
                return illuminati::md::MICEX_CURR;
            if (token == "MICEX_FO")
                return illuminati::md::MICEX_FO;
            if (token == "FORTS_F")
                return illuminati::md::FORTS_F;
            if (token == "FORTS_O")
                return illuminati::md::FORTS_O;
            if (token == "MCX_FUTCOM")
                return illuminati::md::MCX_FUTCOM;
            if (token == "NSE_CDS")
                return illuminati::md::NSE_CDS;
            if (token == "BSE")
                return illuminati::md::BSE;
            if (token == "MCX_SX")
                return illuminati::md::MCX_SX;
            if (token == "REUTERS_GLOBAL")
                return illuminati::md::REUTERS_GLOBAL;
            if (token == "LSE_IOB")
                return illuminati::md::LSE_IOB;
            if (token == "BRAZIL")
                return illuminati::md::BRAZIL;
            if (token == "FLEXTRADE")
                return illuminati::md::FLEXTRADE;
            if (token == "DGCX")
                return illuminati::md::DGCX;
            if (token == "RDM")
                return illuminati::md::RDM;
            if (token == "PUMA_FXFU")
                return illuminati::md::PUMA_FXFU;
            if (token == "PUMA_FXEQOPT")
                return illuminati::md::PUMA_FXEQOPT;
            if (token == "PUMA_IRFU")
                return illuminati::md::PUMA_IRFU;
            if (token == "PUMA_IROPT")
                return illuminati::md::PUMA_IROPT;
            if (token == "PUMA_CMFU")
                return illuminati::md::PUMA_CMFU;
            if (token == "PUMA_CMOPT")
                return illuminati::md::PUMA_CMOPT;
            if (token == "PUMA_EQFU")
                return illuminati::md::PUMA_EQFU;
            if (token == "PUMA_SPFU")
                return illuminati::md::PUMA_SPFU;
            if (token == "PUMA_FXSPOT")
                return illuminati::md::PUMA_FXSPOT;
            if (token == "PUMA_EQIX")
                return illuminati::md::PUMA_EQIX;
            if (token == "PLAZA_FUT")
                return illuminati::md::PLAZA_FUT;
            if (token == "KOSPI_FNO")
                return illuminati::md::KOSPI_FNO;
            if (token == "KOSPI_EQUITY")
                return illuminati::md::KOSPI_EQUITY;
            if (token == "PUMA_EQ_MAT")
                return illuminati::md::PUMA_EQ_MAT;
            if (token == "PUMA_EQ_OPT_MAT")
                return illuminati::md::PUMA_EQ_OPT_MAT;
            if (token == "PUMA_EQ_OIL")
                return illuminati::md::PUMA_EQ_OIL;
            if (token == "PUMA_EQ_OPT_OIL")
                return illuminati::md::PUMA_EQ_OPT_OIL;
            if (token == "PUMA_EQ_FIN")
                return illuminati::md::PUMA_EQ_FIN;
            if (token == "PUMA_EQ_OPT_FIN")
                return illuminati::md::PUMA_EQ_OPT_FIN;
            if (token == "PUMA_EQ_GD")
                return illuminati::md::PUMA_EQ_GD;
            if (token == "PUMA_EQ_OPT_GD")
                return illuminati::md::PUMA_EQ_OPT_GD;
            if (token == "PUMA_EQ_OTC")
                return illuminati::md::PUMA_EQ_OTC;
            if (token == "PUMA_EQ_OPT_IBOV")
                return illuminati::md::PUMA_EQ_OPT_IBOV;
            if (token == "PUMA_EQ")
                return illuminati::md::PUMA_EQ;
            if (token == "PUMA_FU")
                return illuminati::md::PUMA_FU;
            if (token == "NYSE")
                return illuminati::md::NYSE;
            if (token == "ARCA")
                return illuminati::md::ARCA;
            if (token == "PACF")
                return illuminati::md::PACF;
            if (token == "NQEX")
                return illuminati::md::NQEX;
            if (token == "NQNM")
                return illuminati::md::NQNM;
            if (token == "NASDAQ")
                return illuminati::md::NASDAQ;
            if (token == "IB_GLOBAL")
                return illuminati::md::IB_GLOBAL;
            if (token == "CHINA")
                return illuminati::md::CHINA;
            if (token == "BATS")
                return illuminati::md::BATS;
            if (token == "INET")
                return illuminati::md::INET;
            if (token == "CBSX")
                return illuminati::md::CBSX;
            if (token == "CME")
                return illuminati::md::CME;
            if (token == "LME")
                return illuminati::md::LME;
            if (token == "FASTMD")
                return illuminati::md::FASTMD;
            if (token == "CHINA_SHFE")
                return illuminati::md::CHINA_SHFE;
            if (token == "CHINA_CFFEX")
                return illuminati::md::CHINA_CFFEX;
            if (token == "CHINA_ZCE")
                return illuminati::md::CHINA_ZCE;
            if (token == "CHINA_CZCE")
                return illuminati::md::CHINA_ZCE;
            if (token == "CHINA_DCE")
                return illuminati::md::CHINA_DCE;
            if (token == "CHINA_GFEX")
                return illuminati::md::CHINA_GFEX;
            if (token == "SGX")
                return illuminati::md::SGX;
            if (token == "OSE")
                return illuminati::md::OSE;
            if (token == "MCASTNSE_FO")
                return illuminati::md::MCASTNSE_FO;
            if (token == "MCASTNSE_CDS")
                return illuminati::md::MCASTNSE_CDS;
            if (token == "MCASTNSE_CM")
                return illuminati::md::MCASTNSE_CM;
            if (token == "CHINA_SH")
                return illuminati::md::CHINA_SH;
            if (token == "CHINA_SZ")
                return illuminati::md::CHINA_SZ;
            if (token == "CHINA_INE")
                return illuminati::md::CHINA_INE;
            return illuminati::md::EXCHANGE_UNKNOWN;
        }

    } // md
} // illuminaiti

#endif

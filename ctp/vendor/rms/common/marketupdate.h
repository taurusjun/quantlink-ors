#ifndef _INFRA_MD_BASE_H
#define _INFRA_MD_BASE_H

#include <stdint.h>
#include <time.h>
#include <utility>
#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include "stringutils.h"
#include "orderresponse.h"

const int32_t DEPTH_OF_INT = 20;

namespace ors
{
    namespace infra
    {
        typedef std::pair<int32_t, int32_t> element;

        //        enum ExchangeType
        //        {
        //                NSE_FO,
        //                NSE_CM,
        //                NSE_CDS
        //        };

        enum MarketUpdateType
        {
            ORDERENTRY,
            // NEWORDER,
            MODIFY,
            TRADE,
            CANCEL,
            MISMATCH,
            NONE,
            MARKETSTATUS
        };

        struct MarketUpdate
        {
            MarketUpdate()
            {
                memset(this, 0, sizeof(MarketUpdate));
            }
            ExchangeType m_exchType;
            MarketUpdateType m_updateType;
            int32_t m_newPrice;
            int32_t m_newQuant;
            int32_t m_oldPrice;
            int32_t m_oldQuant;
            char m_symbol[50];
            char m_side;
            int32_t m_updateCount;
            element m_updates[DEPTH_OF_INT];
            uint64_t m_exchTS;
            uint64_t m_timestamp;
            uint64_t m_seqnum;
            bool m_endPkt;

            void print()
            {
                std::cout << "Side=" << m_side << "; updateCount=" << m_updateCount << "; m_seqnum=" << m_seqnum << std::endl;
            }

            std::string toString() const
            {
                std::ostringstream css;
                css << m_symbol << "; T=" << m_updateType
                    << "; NEWP=" << m_newPrice << "; NEWQ=" << m_newQuant
                    << "; OLDP=" << m_oldPrice << "; OLDQ=" << m_oldQuant
                    << "; SIDE=" << m_side << "; UPCNT=" << m_updateCount
                    << "; EXTS=" << m_exchTS << "; LTS=" << m_timestamp
                    << "; SEQ= " << m_seqnum << "; ENDPKT=" << m_endPkt << "\n";

                for (int i = 0; i < DEPTH_OF_INT; i++)
                {
                    css << "P = " << m_updates[i].first << "; Q = " << m_updates[i].second << "\n";
                }

                return css.str();
            }

            MarketUpdate operator-(const MarketUpdate &other)
            {
                MarketUpdate delta;

                if (strcmp(::ors::trim(m_symbol).c_str(), ::ors::trim(other.m_symbol).c_str()) == 0)
                {
                    strcpy(delta.m_symbol, m_symbol);
                }
                else
                {
                    throw std::string("MISMATCH UPDATES");
                }

                if (m_side == other.m_side)
                {
                    delta.m_side = 'M';
                }
                else
                {
                    throw std::string("MISMATCH SIDES");
                }

                if (m_updateType == other.m_updateType)
                {
                    delta.m_updateType = m_updateType;
                }
                else
                {
                    delta.m_updateType = MISMATCH;
                }

                /* if( m_newPrice == other.m_newPrice == 0 ) */
                /*   throw std::string( "newprice and other newprice are 0" ); */
                delta.m_newPrice = m_newPrice - other.m_newPrice;

                /* if( m_newQuant == other.m_newQuant == 0 ) */
                /*   throw std::string( "newquant and other new quant are 0" ); */
                delta.m_newQuant = m_newQuant - other.m_newQuant;

                /* if( m_oldPrice == other.m_oldPrice == 0 ) */
                /*   throw std::string( "old price and other old price are 0" ); */
                delta.m_oldPrice = m_oldPrice - other.m_oldPrice;

                /* if( m_oldQuant == other.m_oldQuant == 0 ) */
                /*   throw std::string( "old quant and other old quant are 0" ); */
                delta.m_oldQuant = m_oldQuant - other.m_oldQuant;

                delta.m_updateCount = m_updateCount - other.m_updateCount;

                for (int i = 0; i < DEPTH_OF_INT; i++)
                {
                    delta.m_updates[i].first = m_updates[i].first - other.m_updates[i].first;
                    delta.m_updates[i].second = m_updates[i].second - other.m_updates[i].second;
                }

                delta.m_exchTS = m_exchTS - other.m_exchTS;
                delta.m_timestamp = m_timestamp - other.m_timestamp;
                delta.m_seqnum = m_seqnum - other.m_seqnum;

                return delta;
            }
        };

        /* For testing of bindelta
         * This should be part of marketupdate file
         * should be maintained at one place and manually byte aligned to 8 bytes
         */
        struct SendPacket
        {
            MarketUpdateType m_updateType;
            int32_t m_newPrice;
            int32_t m_newQuant;
            int32_t m_oldPrice;
            int32_t m_oldQuant;
            int32_t m_updateCount;
            element m_updates[20];
            char m_side;
            bool m_endPkt;
            char m_filler[6];

            SendPacket()
            {
                memset(this, 0, sizeof(SendPacket));
            }

            void copyFrom(const MarketUpdate *up)
            {
                this->m_updateType = up->m_updateType;
                this->m_newPrice = up->m_newPrice;
                this->m_newQuant = up->m_newQuant;
                this->m_oldPrice = up->m_oldPrice;
                this->m_oldQuant = up->m_oldQuant;
                this->m_side = up->m_side;
                this->m_endPkt = up->m_endPkt;
                this->m_updateCount = up->m_updateCount;
                memcpy(this->m_updates, up->m_updates, sizeof(this->m_updates));
            }

            void copyTo(MarketUpdate *up)
            {
                up->m_updateType = this->m_updateType;
                up->m_newPrice = this->m_newPrice;
                up->m_newQuant = this->m_newQuant;
                up->m_oldPrice = this->m_oldPrice;
                up->m_oldQuant = this->m_oldQuant;
                up->m_side = this->m_side;
                up->m_endPkt = this->m_endPkt;
                up->m_updateCount = this->m_updateCount;
                memcpy(up->m_updates, this->m_updates, sizeof(up->m_updates));
            }

            std::string toString() const
            {
                std::ostringstream css;
                css << "; T=" << m_updateType
                    << "; NEWP=" << m_newPrice << "; NEWQ=" << m_newQuant
                    << "; OLDP=" << m_oldPrice << "; OLDQ=" << m_oldQuant
                    << "; SIDE=" << m_side << "; UPCNT=" << m_updateCount
                    << "; ENDPKT=" << m_endPkt << "\n";

                for (int i = 0; i < DEPTH_OF_INT; i++)
                {
                    css << "P = " << m_updates[i].first << "; Q = " << m_updates[i].second << "\n";
                }

                return css.str();
            }
        };
    }
}

#endif

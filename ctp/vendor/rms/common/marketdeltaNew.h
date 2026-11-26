#ifndef _INFRA_MDDELTANEW_MSGS_H
#define _INFRA_MDDELTANEW_MSGS_H

#include <string.h>
#include <sstream>
#include <iostream>

namespace ors
{
    namespace infra
    {

        const int V3DATASIZE = 1024;
        const int SYMBOL_SZ = 50;

        enum DataState
        {
            ALL_GOOD,
            MISSING_LTS
        };

        struct DataHeaderNew
        {
            uint64_t exchTime;
            uint64_t timestamp;
            uint64_t sequenceNumber;
            uint64_t rptsequenceNumber;
            uint64_t tokenId;
            int32_t dataSize;
            unsigned char exchangeCodeName;

            void print(void)
            {
                std::cout
                    << "\n+exchTime: " << exchTime
                    << "\n+timestamp:" << timestamp
                    << "\n+seqNo:    " << sequenceNumber
                    << "\n+rptseqNo: " << rptsequenceNumber
                    << "\n+tokenId:  " << tokenId
                    << "\n+exchcode: " << (int)exchangeCodeName
                    << "\n+dataSize: " << dataSize
                    << std::endl;
            }
        };

        struct DataWrapperV3 : public DataHeaderNew
        {
            char symbol[SYMBOL_SZ - 2];
            uint16_t symbolID;
            char data[V3DATASIZE];
            DataState dataState;

            DataWrapperV3() : dataState(ALL_GOOD)
            {
            }

            void print(void)
            {
                DataHeaderNew::print();
                std::cout
                    << "\n+symbol:        " << symbol
                    << "\n+dataState:     " << dataState
                    << std::endl;
            }
        };

        struct MetadataHeaderNew
        {
            uint64_t StartOffset;
            uint64_t EndOffset;
            char Symbol[SYMBOL_SZ];

            MetadataHeaderNew() { memset(this, 0, sizeof(MetadataHeaderNew)); }

            void print(void)
            {
                std::cout
                    << "\n+Symbol:     " << Symbol
                    << "\n+StartOffset:" << StartOffset
                    << "\n+EndOffset:  " << EndOffset
                    << std::endl;
            }
        };

        struct TSData
        {
            char m_symbol[SYMBOL_SZ];
            uint64_t m_rptseqnum;
            uint64_t m_seqnum;
            uint64_t m_timestamp;

            std::string toString(void)
            {
                std::ostringstream ss;
                ss << "|Symbol=" << m_symbol
                   << "|LocalTS=" << m_timestamp
                   << "|Seq=" << m_seqnum
                   << "|RptSeqnum=" << m_rptseqnum;
                return ss.str();
            }

            void print(void)
            {
                std::cout << "\n|Symbol: " << m_symbol
                          << "\n|RptSeqnum: " << m_rptseqnum
                          << "\n|Seq: " << m_seqnum
                          << "\n|timestamp: " << m_timestamp
                          << std::endl;
            }
        };

        struct AdditionalData : public DataHeaderNew
        {
            char m_symbol[SYMBOL_SZ];

            void print(void)
            {
                std::cout << "\n|Symbol: " << m_symbol << std::endl;
                DataHeaderNew::print();
            }
        };

    } // namespace infra
} // namespace ors

#endif

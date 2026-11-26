#ifndef __Common_Utils_H
#define __Common_Utils_H

#include "marketdelta.h"
#include "orderresponse.h"
#include "stringutils.h"
#include <utility>
#include <random>
#include "c11compatible.h"
#include <sys/time.h>

// typedef CircularQ(illuminati::md::MarketUpdateNew, illuminati::storagepolicy::SHMStorage,
// 		  INT(MICEX_MD_SHM_SIZE), INT(MICEX_MD_SHM_KEY), INT(IPC_CREAT | 0666)) MdShmQueueWriter;
// typedef CircularQ(illuminati::md::MarketUpdateNew, illuminati::storagepolicy::SHMStorage,
// 		  INT(MICEX_MD_SHM_SIZE), INT(MICEX_MD_SHM_KEY), INT(0666)) MdShmQueueReader;

namespace illuminati
{
    class CommonUtils
    {
    public:
        static inline std::string GetCurrTime(uint64_t TS)
        {
            if (TS == 0)
            {
                // time_t currtime = time(NULL) ;
                // struct tm* currTime = localtime( &currtime ) ;
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return GetCurrTime(((uint64_t)(tv.tv_sec + 19800) * 1000 + ((uint64_t)tv.tv_usec / 1000)));
            }

            char currtime[100];
            uint64_t exchTS = TS;
            exchTS /= 1000;
            exchTS %= 86400;
            sprintf(currtime, "%.2ld:%.2ld:%.2ld:%.3ld", exchTS / 3600, (exchTS % 3600) / 60, (exchTS % 3600) % 60, TS % 1000);
            return std::string(currtime);
        }

        static int getPacketSize(illuminati::infra::Updates updateType)
        {
            int size = 0;
            switch (updateType)
            {
            case illuminati::infra::OrderBookUp:
                size = sizeof(illuminati::infra::OBPacket);
                break;
            case illuminati::infra::Insert:
                size = sizeof(illuminati::infra::InsertUpPacket);
                break;
            case illuminati::infra::Delete:
                size = sizeof(illuminati::infra::DeleteUpPacket);
                break;
            case illuminati::infra::Replace:
                size = sizeof(illuminati::infra::ReplaceUpPacket);
                break;
            case illuminati::infra::Modify:
                size = sizeof(illuminati::infra::ModifyUpPacket);
                break;
            case illuminati::infra::Trade:
                size = sizeof(illuminati::infra::TradeUpPacket);
                break;
            case illuminati::infra::TimeUpdate:
                size = sizeof(illuminati::infra::TimestampUpPacket);
                break;
            // TODO: hack for bindelta, this should be changed
            default:
                size = updateType;
                break;
            }
            return size;
        }

        static illuminati::infra::ExchangeType getExchangeTypeFromFile(std::string dataFileName)
        {
            if (dataFileName.find("CME") != std::string::npos)
                return illuminati::infra::ExchangeType::eCME;
            else if (dataFileName.find("NYSE") != std::string::npos)
                return illuminati::infra::ExchangeType::eNYSE;
            else if (dataFileName.find("ARCA") != std::string::npos)
                return illuminati::infra::ExchangeType::eARCA;

            std::string::size_type pos = dataFileName.rfind('/');
            std::string type = dataFileName.substr(pos + 1);
            pos = type.find('_');
            type = type.substr(0, pos);

            illuminati::infra::ExchangeType xType = illuminati::infra::NOT_NSE;
            if (strcmp(type.c_str(), "NSECM") == 0)
                xType = illuminati::infra::NSE_CM;
            else if (strcmp(type.c_str(), "NSEFO") == 0)
                xType = illuminati::infra::NSE_FO;
            else if (strcmp(type.c_str(), "NSECDS") == 0)
                xType = illuminati::infra::NSE_CDS;

            return xType;
        }

        static double getDouble(std::string doubleStr)
        {
            if (trim(doubleStr).length() == 0)
                return 0.0;
            return std::stod(doubleStr);
        }

        static int getInt(std::string intStr)
        {
            if (trim(intStr).length() == 0)
                return 0;
            return std::stoi(intStr);
        }

        template <typename Engine = std::mt19937, typename Distribution = std::uniform_int_distribution<uint32_t>>
        static uint32_t generateRandomNumber()
        {
            std::random_device rd;
            Engine engine(rd());
            Distribution dis;
            return dis(engine);
        }
    };
}
#endif

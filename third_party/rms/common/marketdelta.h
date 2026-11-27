#ifndef _INFRA_MDDELTA_MSGS_H
#define _INFRA_MDDELTA_MSGS_H

#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <string.h>
#include <string>
#include <sstream>
#include "marketupdate.h"
#include "orderresponse.h"
#include "marketdeltaNew.h"

namespace ors
{
    namespace infra
    {
#define LEVELS 20

        enum Updates
        {
            OrderBookUp,
            Insert,
            Delete,
            Replace,
            Modify,
            Trade,
            EndPkt,
            None,
            Dummy,
            TimeUpdate
        };

        struct Header
        {
            Header()
            {
                Type = None;
            }
            uint64_t ExchTime;
            uint64_t Start_Offset;
            Updates Type; //=> small int
            char Symbol[32];
            char Side;
            void print()
            {
                std::cout << "Exchtime=" << ExchTime << "; Start_Offset=" << Start_Offset << "; Type=" << Type << "; Symbol=" << Symbol << "; Side=" << Side << std::endl;
            }

            std::string toString()
            {
                std::ostringstream css;
                css << "Exchtime=" << ExchTime << "; Start_Offset=" << Start_Offset << "; Type=" << Type << "; Symbol=" << Symbol << "; Side=" << Side << std::endl;
                return css.str();
            }
        };

        struct HeaderWrap
        {
            Header actualdata;
            ExchangeType exchange_type;
        };

        struct OB
        {
            MarketUpdateType market_update_type;
            element Ob[LEVELS];
        };

        struct DeleteUp
        {
            MarketUpdateType market_update_type;
            int32_t Level;
            int32_t Price;
            int32_t Qty;
        };

        struct TradeUp
        {
            MarketUpdateType market_update_type;
            int32_t TradePrice;
            int32_t TradeQty;
            int32_t NewPrice;
            int32_t NewQty;
        };

        struct InsertUp
        {
            MarketUpdateType market_update_type;
            int32_t Level;
            int32_t Price;
            int32_t Qty;
        };

        struct ModifyUp
        {
            ModifyUp()
            {
                OldType = None;
                NewType = None;
            }
            MarketUpdateType market_update_type;
            Updates OldType;
            int32_t OldLevel;
            int32_t OldPrice;
            int32_t OldQty;
            Updates NewType;
            int32_t NewLevel;
            int32_t NewPrice;
            int32_t NewQty;
        };

        struct ReplaceUp
        {
            MarketUpdateType market_update_type;
            int32_t Level;
            int32_t Price;
            int32_t Qty;
        };

        static size_t OBSize = sizeof(OB);
        static size_t DeleteSize = sizeof(DeleteUp);
        static size_t TradeSize = sizeof(TradeUp);
        static size_t ReplaceSize = sizeof(ReplaceUp);
        static size_t InsertSize = sizeof(InsertUp);
        static size_t ModifySize = sizeof(ModifyUp);
        static size_t HeaderSize = sizeof(Header);
        static size_t ElementSize = sizeof(element);

        struct DataWrapperV1
        {
            Header *h;
            void *d;
        };

        struct DataWrapperV2
        {
            uint64_t exchTime;
            uint64_t localTime;
            ExchangeType exchange_type;
            char data[256];
            Updates type;
            char symbol[32];
            uint64_t sequenceNumber;
        };

        struct MetadataHeader
        {
            uint64_t StartOffset;
            uint64_t EndOffset;
            char Symbol[32];
        };

        struct DataHeader
        {
            uint64_t ExchTime;
            Updates Type; //=> small int
            char Side;
            uint64_t sequenceNumber;
        };

        //        uint64_t       exchTime ;
        //        uint64_t       timestamp;
        //        uint64_t       sequenceNumber;
        //        uint64_t       rptsequenceNumber;
        //        uint64_t       tokenId;
        //        int32_t        dataSize;
        //        unsigned char  exchangeCodeName;

        struct OBPacket
        {
#if defined(_USE_OLD_STRUCT)
            DataHeader Header;
#else
            DataHeaderNew Header;
#endif
            MarketUpdateType market_update_type;
            element Ob[LEVELS];
            int32_t NewPrice;
            int32_t NewQuant;
            int32_t OldPrice;
            int32_t OldQuant;
        };

        struct DeleteUpPacket
        {
#if defined(_USE_OLD_STRUCT)
            DataHeader Header;
#else
            DataHeaderNew Header;
#endif
            MarketUpdateType market_update_type;
            int32_t Level;
            int32_t Price;
            int32_t Qty;
        };

        struct TradeUpPacket
        {
#if defined(_USE_OLD_STRUCT)
            DataHeader Header;
#else
            DataHeaderNew Header;
#endif
            MarketUpdateType market_update_type;
            int32_t TradePrice;
            int32_t TradeQty;
            int32_t NewPrice;
            int32_t NewQty;
        };

        struct InsertUpPacket
        {
#if defined(_USE_OLD_STRUCT)
            DataHeader Header;
#else
            DataHeaderNew Header;
#endif
            MarketUpdateType market_update_type;
            int32_t Level;
            int32_t Price;
            int32_t Qty;
        };

        struct ModifyUpPacket
        {
#if defined(_USE_OLD_STRUCT)
            DataHeader Header;
#else
            DataHeaderNew Header;
#endif
            ModifyUpPacket()
            {
                OldType = None;
                NewType = None;
            }
            MarketUpdateType market_update_type;
            Updates OldType;
            int32_t OldLevel;
            int32_t OldPrice;
            int32_t OldQty;
            Updates NewType;
            int32_t NewLevel;
            int32_t NewPrice;
            int32_t NewQty;
            int32_t OldQtyDelta;
            int32_t NewQtyDelta;
        };

        struct ReplaceUpPacket
        {
#if defined(_USE_OLD_STRUCT)
            DataHeader Header;
#else
            DataHeaderNew Header;
#endif
            MarketUpdateType market_update_type;
            int32_t Level;
            int32_t Price;
            int32_t Qty;
        };

        struct TimestampUpPacket
        {
            DataHeader Header;
        };

    }
}

#endif

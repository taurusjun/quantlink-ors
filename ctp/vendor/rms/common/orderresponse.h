/*
 * Company: way2wealth-ors
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-ors.com/
 */
#ifndef _ILLUMINATI_ORDERRESPONSE_H
#define _ILLUMINATI_ORDERRESPONSE_H

#include "marketupdateNew.h"
#include "errorcodes.h"
#include <sstream>
#include "serializer.h"

namespace ors
{
    namespace infra
    {
        const int32_t MAX_ORS_CLIENTS = 250;
        const int32_t MAX_ACCNTID_LEN = 10;
        const int32_t MAX_TRDBOARD_LEN = 10;
        const char ACCNT_DEFAULT[] = "          ";
        const int32_t MAX_INSTRNAME_SIZE = 32;
        const int32_t MAX_TRADE_ID_SIZE = 21;
        const int32_t MAX_PRODUCT_SIZE = 32;

        const uint32_t NO_FILL_INDICATOR = 0;
        const uint32_t PARTIAL_FILL_INDICATOR = 1;
        const uint32_t FULL_FILL_INDICATOR = 2;

        enum ExchangeType
        {
            NSE_FO,
            NSE_CM,
            NSE_CDS,
            MICEX_FOND,
            MICEX_CURR,
            MCX,
            eCME,
            eLME,
            eNYSE,
            eARCA,
            NOT_NSE,
            REQUEST_MSG_EXCHG,
            RESPONSE_MSG_EXCHG,
            ExchangeTypeNum // keep this at the end, used to calculate number of elements in this enum
        };

        enum OrderType
        {
            LIMIT = 1,
            MARKET = 2,
            WEIGHTAVG = 3,
            CONDITIONAL_LIMIT_PRICE = 4,
            BEST_PRICE = 5,
            OrderTypeNum // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string OrderTypeStr[] = {"", "LIMIT", "MARKET", "WEIGHTAVG", "CONDLIMITPRICE", "BESTPRICE"};

        enum RequestType
        {
            NEWORDER,       // 新订单
            MODIFYORDER,    // 修改订单
            CANCELORDER,    // 撤单
            ORDERSTATUS,    // 订单状态
            SESSIONMSG,     // 查询交易时段
            HEARTBEAT,      // 心跳
            OPTEXEC,        // 期权行权指令
            OPTEXEC_CANCEL, // 期权撤销行权指令
            RequestTypeNum  // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string RequestTypeStr[] = {"NEWORDER", "MODIFYORDER", "CANCELORDER", "ORDERSTATUS", "SESSIONMSG", "HEARTBEAT", "OPTEXEC", "OPTEXEC_CANCEL"};

        enum OrderDuration
        {
            DAY,
            IOC,
            FOK,
            COUNTER, // For forts (these orders are removed after auction end)
            FAK,
            OrderDurationNum // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string OrderDurationStr[] = {"DAY", "IOC", "FOK", "COUNTER", "FAK"};

        enum PriceType
        {
            PERCENTAGE = 1,
            PERUNIT = 2,
            YIELD = 9,
            PriceTypeNum // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string PriceTypeStr[]{"PERCENTAGE", "PERUNIT", "YIELD"};

        enum InstrumentType
        {
            STK,
            FUT,
            OPT,
            XXX,
            InstrumentTypeNum // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string InstrumentTypeStr[]{"STK", "FUT", "OPT", "XXX"};

        struct ContractDescription
        {
            char InstrumentName[MAX_INSTRNAME_SIZE]; /* FUTSTK, FUTIDX, OPTSTK, OPTIDX */
            char Symbol[md::MAX_SYMBOL_SIZE];
            int32_t ExpiryDate;
            int32_t StrikePrice;
            char OptionType[2]; /* XX - for futures */
            int16_t CALevel;    /* always 0 */
        };

        enum PositionDirection
        {
            OPEN = 10,
            CLOSE,
            CLOSE_INTRADAY,
            POS_ERROR,
            PositionDirectionNum // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string PositionDirectionStr[]{"", "", "", "", "", "", "", "", "", "", "OPEN",
                                                 "CLOSE", "CLOSE_INTRADAY", "POS_ERROR"};

        /*!
         * ----------------------------------------------
         * From strategy to ORS
         * ----------------------------------------------
         */
        struct RequestMsg
        {
            ContractDescription Contract_Description;
            RequestType Request_Type;
            OrderType OrdType;
            OrderDuration Duration;
            PriceType PxType;
            PositionDirection PosDirection;
            uint32_t OrderID;
            int32_t Token;
            int32_t Quantity;
            int32_t QuantityFilled;
            int32_t DisclosedQnty;
            double Price;
            uint64_t TimeStamp;
            char AccountID[MAX_ACCNTID_LEN + 1]; /*! adding one extra byte for the null byte */
            unsigned char Transaction_Type;
            unsigned char Exchange_Type;
            char padding[20];
            char Product[MAX_PRODUCT_SIZE]{0};
            int StrategyID{0};

            std::string toString() const
            {
                std::ostringstream ss;
                ss << "\n+RequestType= " << Request_Type << " (0=new,1=mod,2=cxl,3=status)"
                   << "\n+Product    = " << Product
                   << "\n+StrategyID = " << StrategyID
                   << "\n+Symbol     = " << Contract_Description.Symbol
                   << "\n+Side       = " << Transaction_Type
                   << "\n+Quantity   = " << Quantity
                   << "\n+Price      = " << Price
                   << "\n+OrderType  = " << OrdType << " (1=limit,2=market)"
                   << "\n+OrderID    = " << OrderID
                   << "\n+Token      = " << Token
                   << "\n+AccountID  = " << AccountID
                   << "\n+Exch       = " << (int)Exchange_Type
                   << "\n+TimeStamp       = " << TimeStamp
                   << "\n";
                return ss.str();
            }
            std::string toStringLine() const
            {
                std::ostringstream ss;
                ss << "|RequestType:" << Request_Type
                   << "|Symbol:" << Contract_Description.Symbol
                   << "|Side:" << Transaction_Type
                   << "|Quantity:" << Quantity
                   << "|Price:" << Price
                   << "|OrderType:" << OrdType
                   << "|OrderID:" << OrderID
                   << "|Token:" << Token
                   << "|AccountID:" << AccountID
                   << "|Exch:" << (int)Exchange_Type
                   << "|TimeStamp:" << TimeStamp;
                return ss.str();
            }

            void print()
            {
                std::cout << toString();
            }
            void orderReqPrint()
            {
                print();
            }

            void setAccountId(const char *input)
            {
                bool copy = true;
                int32_t i;

                for (i = 0; i < MAX_ACCNTID_LEN; i++)
                {
                    if (0 == (int)*(input + i))
                    {
                        copy = false;
                    }

                    if (copy)
                    {
                        AccountID[i] = *(input + i);
                    }
                    else
                    {
                        AccountID[i] = ' ';
                    }
                }

                AccountID[MAX_ACCNTID_LEN] = '\0';
            }

            static void getFormattedAccountId(std::string &accountID)
            {
                RequestMsg req;
                accountID.resize(MAX_ACCNTID_LEN, 0);
                req.setAccountId(accountID.c_str());
                accountID = req.AccountID;
            }

            void setSymbol(const char *input)
            {
                bool copy = true;
                int32_t i;

                for (i = 0; i < md::MAX_SYMBOL_SIZE; i++)
                {
                    if (0 == (int)*(input + i))
                    {
                        copy = false;
                    }

                    if (copy)
                    {
                        Contract_Description.Symbol[i] = *(input + i);
                    }
                    else
                    {
                        break;
                        // Contract_Description.Symbol[i] = ' ';
                    }
                }

                Contract_Description.Symbol[i] = '\0';
            }

            void setOpenClosePosition(PositionDirection input = OPEN)
            {
                // switch is just needed for validity of the input
                switch (input)
                {
                case CLOSE:
                    PosDirection = CLOSE;
                    break;
                case CLOSE_INTRADAY:
                    PosDirection = CLOSE_INTRADAY;
                    break;
                case OPEN:
                    PosDirection = OPEN;
                    break;
                default:
                    // log some error
                    break;
                }
            }

            PositionDirection getDirection()
            {
                switch (PosDirection)
                {
                case CLOSE:
                    return CLOSE;
                case CLOSE_INTRADAY:
                    return CLOSE_INTRADAY;
                case OPEN:
                    return OPEN;
                default:
                    return POS_ERROR;
                }
            }

        } __attribute__((aligned(64)));

        struct RequestMsgSerializer : public Serializer
        {
            RequestMsg *request_;
            bool delete_on_destruction_;

            RequestMsgSerializer(RequestMsg &request) : delete_on_destruction_(false)
            {
                request_ = &request;
            }

            std::string toString(RequestType request) const
            {
                if (request >= 0 && request < RequestTypeNum)
                {
                    return RequestTypeStr[request];
                }
                else
                {
                    return "UNKNOWN";
                }
            }

            std::string toString(OrderType type) const
            {
                if (type >= 0 && type < OrderTypeNum)
                {
                    return OrderTypeStr[type];
                }
                else
                {
                    return "UNKNOWN";
                }
            }

            std::string toString(OrderDuration duration) const
            {
                if (duration >= 0 && duration < OrderDurationNum)
                {
                    return OrderDurationStr[duration];
                }
                else
                {
                    return "UNKNOWN";
                }
            }

            std::string serialize() const
            {
                char buff[1024];
                sprintf(buff, "RequestType:%s|OrderID:%d|Token:%d|Symbol:%s|Side:%c|Price:%f|Quantity:%d|TimeStamp:%lu|Duration:%s|OrderType:%s|Account:%s|Exchange:%d",
                        toString(request_->Request_Type).c_str(), request_->OrderID, request_->Token,
                        request_->Contract_Description.Symbol, request_->Transaction_Type,
                        request_->Price, request_->Quantity, request_->TimeStamp,
                        toString(request_->Duration).c_str(), toString(request_->OrdType).c_str(),
                        request_->AccountID, (int)request_->Exchange_Type);

                return buff;
            }

            Serializer &clone() const
            {
                RequestMsg *req = new RequestMsg();
                *req = *request_;
                RequestMsgSerializer *obj = new RequestMsgSerializer(*req);
                obj->delete_on_destruction_ = true;
                return *obj;
            }

            ~RequestMsgSerializer()
            {
                if (delete_on_destruction_)
                {
                    delete request_;
                }
            }
        };

        /*!
         * --------------------------------------------------------------
         * From ORS to strategy
         * --------------------------------------------------------------
         */
        enum ResponseType
        {
            NEW_ORDER_CONFIRM,  // 订单确认
            NEW_ORDER_FREEZE,   // 
            MODIFY_ORDER_CONFIRM,// 修改确认
            CANCEL_ORDER_CONFIRM,   // 已撤
            TRADE_CONFIRM,      // 已成
            ORDER_ERROR,        // 废单
            MODIFY_ORDER_REJECT,
            CANCEL_ORDER_REJECT, // 撤废
            ORS_REJECT,         // ORS废单
            RMS_REJECT,         // 风控废单
            SIM_REJECT,         // 模拟废单
            BUSINESS_REJECT,
            MODIFY_ORDER_PENDING,
            CANCEL_ORDER_PENDING,
            ORDERS_PER_DAY_LIMIT_REJECT,
            ORDERS_PER_DAY_LIMIT_WARNING,
            ORDER_EXPIRED,
            STOP_LOSS_WARNING,
            NULL_RESPONSE,
            ResponseTypeNum // keep this at the end, used to calculate number of elements in this enum
        };

        const std::string ResponseTypeStr[]{"NEW_ORDER_CONFIRM", "NEW_ORDER_FREEZE", "MODIFY_ORDER_CONFIRM",
                                            "CANCEL_ORDER_CONFIRM", "TRADE_CONFIRM", "ORDER_ERROR", "MODIFY_ORDER_REJECT", "CANCEL_ORDER_REJECT",
                                            "ORS_REJECT", "RMS_REJECT", "SIM_REJECT", "BUSINESS_REJECT", "MODIFY_ORDER_PENDING", "CANCEL_ORDER_PENDING",
                                            "ORDERS_PER_DAY_LIMIT_REJECT", "ORDERS_PER_DAY_LIMIT_WARNING", "ORDER_EXPIRED",
                                            "STOP_LOSS_WARNING", "NULL_RESPONSE"};

        enum SubResponseType
        {
            NULL_RESPONSE_MIDDLE,
            ORDER_REJECT_MIDDLE,
            MODIFY_REJECT_MIDDLE,
            CANCEL_ORDER_REJECT_MIDDLE
        };

        enum class OpenCloseType : char
        {
            NULL_TYPE = 0,
            OPEN = 1,
            CLOSE = 2,
            CLOSE_TODAY = 3
        };

        enum class TsExchangeID : char
        {
            NULL_EXCHANGE = 0,
            SHFE = 1,
            INE = 2,
            CZCE = 3,
            DCE = 4,
            CFFEX = 5,
            GFEX = 6,
            SH = 7,
            SZ = 8,
        };

        struct ResponseMsg
        {
        private:
            union double_uint64_t
            {
                uint64_t uint64_t_;
                double double_;
            };

        public:
            ResponseType Response_Type{ResponseType::NULL_RESPONSE};
            SubResponseType Child_Response{SubResponseType::NULL_RESPONSE_MIDDLE};
            uint32_t OrderID{0};
            uint32_t ErrorCode{0};
            int32_t Quantity{0}; // Used only in the case of Trade
            double Price{0};     // Used only in the case of Trade
            uint64_t TimeStamp{0};
            unsigned char Side{0};
            char Symbol[md::MAX_SYMBOL_SIZE]{0};
            char AccountID[MAX_ACCNTID_LEN + 1]{0};
            double ExchangeOrderId{0};                            // Used for NSE
            char ExchangeTradeId[MAX_TRADE_ID_SIZE]{0};           // use for ts
            OpenCloseType OpenClose{OpenCloseType::NULL_TYPE};    // use for ts
            TsExchangeID ExchangeID{TsExchangeID::NULL_EXCHANGE}; // use for ts
            char Product[MAX_PRODUCT_SIZE]{0};                    // add by pqr
            int StrategyID{0};                                    // add by pqr
            ResponseMsg()
            {
                memset(Symbol, '\0', sizeof(Symbol));
                resetErrState();
            }

            uint64_t getExchangeOrderId()
            {
                double_uint64_t fu;
                fu.double_ = ExchangeOrderId;
                return fu.uint64_t_;
            }

            void setExchangeOrderId(uint64_t id)
            {
                double_uint64_t fu;
                fu.uint64_t_ = id;
                ExchangeOrderId = fu.double_;
            }

            void setExchangeOrderId(double id)
            {
                ExchangeOrderId = id;
            }

            inline __attribute__((always_inline)) void resetErrState()
            {
                Response_Type = NULL_RESPONSE;
                ErrorCode = E_NO_ORS_RMS_ERROR;
                Child_Response = NULL_RESPONSE_MIDDLE;
            }

            const char *getResposeTypeDesc() const
            {
                switch (Response_Type)
                {
                case NEW_ORDER_CONFIRM:
                    return "NEW_ORDER_CONFIRM";
                case NEW_ORDER_FREEZE:
                    return "NEW_ORDER_FREEZE";
                case MODIFY_ORDER_CONFIRM:
                    return "MODIFY_ORDER_CONFIRM";
                case CANCEL_ORDER_CONFIRM:
                    return "CANCEL_ORDER_CONFIRM";
                case TRADE_CONFIRM:
                    return "TRADE_CONFIRM";
                case ORDER_ERROR:
                    return "ORDER_ERROR";
                case MODIFY_ORDER_REJECT:
                    return "MODIFY_ORDER_REJECT";
                case CANCEL_ORDER_REJECT:
                    return "CANCEL_ORDER_REJECT";
                case MODIFY_ORDER_PENDING:
                    return "MODIFY_ORDER_PENDING";
                case CANCEL_ORDER_PENDING:
                    return "CANCEL_ORDER_PENDING";
                case ORS_REJECT:
                    return "ORS_REJECT";
                case RMS_REJECT:
                    return "RMS_REJECT";
                case SIM_REJECT:
                    return "SIM_REJECT";
                case ORDERS_PER_DAY_LIMIT_REJECT:
                    return "ORDERS_PER_DAY_LIMIT_REJECT";
                case ORDERS_PER_DAY_LIMIT_WARNING:
                    return "ORDERS_PER_DAY_LIMIT_WARNING";
                case ORDER_EXPIRED:
                    return "ORDER_EXPIRED";
                default:
                    return "Response_Type string not mapped in orderresponse.h";
                }
            }

            void print()
            {
                std::cout << toString() << "\n";
            }

            std::string toString() const
            {
                std::ostringstream css;
                css << "Type:" << Response_Type << "(" << getResposeTypeDesc() << ")|"
                    << "Symbol:" << Symbol << "|"
                    << "Side:" << Side << "|"
                    << "OpenClose:" << (int)OpenClose << "(NONE:0,OPEN:1,CLOSE:2,CLOSE_TODAY:3)|"
                    << "ExchangeID:" << (int)ExchangeID << "(SHFX:1,INE:2,CZCE:3,DCE:4,CFFEX:5,GFEX:6)|"
                    << "OrderID:" << OrderID << "|"
                    << "AccountID:" << AccountID << "|"
                    << "ErrorCode:" << ErrorCode << "|"
                    << "ErrDesc:" << getErrorCodeDesciption(ErrorCode) << "|"
                    << "Quantity:" << Quantity << "|"
                    << "Price:" << Price << "|"
                    << "TimeStamp:" << TimeStamp << "|"
                    << "ExchangeTradeId:" << ExchangeTradeId << "|"
                    << "Exchange_Order_Id:" << std::setprecision(30) << ExchangeOrderId << "|"
                    << "Product:" << Product << "|"
                    << "StrategyID:" << (int)StrategyID << std::endl;
                return css.str();
            }
        };

        struct ResponseMsgSerializer : public Serializer
        {
            ResponseMsg *response_;
            bool delete_on_destruction_;

            ResponseMsgSerializer(ResponseMsg &response) : delete_on_destruction_(false)
            {
                response_ = &response;
            }

            std::string toString(ResponseType type) const
            {
                if (type >= 0 && type < ResponseTypeNum)
                {
                    return ResponseTypeStr[type];
                }
                else
                {
                    return "UNKNOWN";
                }
            }

            std::string serialize() const
            {
                char buff[1024];
                unsigned char side = response_->Side == 'B' || response_->Side == 'S' ? response_->Side : 'N';
                sprintf(buff, "ResponseType:%s|OrderID:%d|Quantity:%d|Price:%f|TimeStamp:%lu|Side:%c|ErrorCode:%d",
                        toString(response_->Response_Type).c_str(), response_->OrderID,
                        response_->Quantity, response_->Price, response_->TimeStamp, side, response_->ErrorCode);
                return std::string(buff);
            }

            Serializer &clone() const
            {
                ResponseMsg *res = new ResponseMsg();
                *res = *response_;
                ResponseMsgSerializer *obj = new ResponseMsgSerializer(*res);
                obj->delete_on_destruction_ = true;
                return *obj;
            }

            ~ResponseMsgSerializer()
            {
                if (delete_on_destruction_)
                {
                    delete response_;
                }
            }
        };
    } // infra
} // ors

#endif

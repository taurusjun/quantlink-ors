/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */
#ifndef _ILLUMINATI_ERROR_CODES_H
#define _ILLUMINATI_ERROR_CODES_H

#include <inttypes.h>
/*!
 * ORS RMS Section
 */
#define E_NO_ORS_RMS_ERROR 0
#define E_ORS_RMS_BASE 100000
#define E_ORS_INVALID_SIDE (E_ORS_RMS_BASE + 1)
#define E_ORS_INVALID_ORDER_TYPE (E_ORS_RMS_BASE + 2)
#define E_ORS_INVALID_DURATION (E_ORS_RMS_BASE + 3)
#define E_ORS_INVALID_REQUEST_TYPE (E_ORS_RMS_BASE + 4)
#define E_ORS_UNKNOWN_TRADING_SESSION (E_ORS_RMS_BASE + 40)
#define E_ORS_RECONNECTING_TO_EXCHANGE (E_ORS_RMS_BASE + 41)
#define E_ORS_SESSION_TERMINATED (E_ORS_RMS_BASE + 42)
#define E_ORS_SESSION_ABORTED (E_ORS_RMS_BASE + 43)
#define E_ORS_SYMBOLMAPPING_NOT_FOUND (E_ORS_RMS_BASE + 44)
#define E_ORS_NOT_CONFIRMED (E_ORS_RMS_BASE + 45)
#define E_ORS_PAUSED (E_ORS_RMS_BASE + 46)
#define E_ORS_QUEUED_ORDER_REJECT (E_ORS_RMS_BASE + 47)
#define E_ORS_EXCHANGE_CANCEL (E_ORS_RMS_BASE + 48)
#define E_ORS_SELF_TRADE (E_ORS_RMS_BASE + 49)
// 新增订单相关错误码
#define E_ORS_NoPositionToClose (E_ORS_RMS_BASE + 50)
#define E_ORS_NoMoneyToOpen (E_ORS_RMS_BASE + 51)
#define E_ORS_InsertOrderTooFast (E_ORS_RMS_BASE + 52)
#define E_ORS_OrderTypeNotSupported (E_ORS_RMS_BASE + 53)
#define E_ORS_OrderFinished (E_ORS_RMS_BASE + 54)
#define E_ORS_PriceOutOfLimit (E_ORS_RMS_BASE + 55)
#define E_ORS_InvalidTradeSection (E_ORS_RMS_BASE + 56)
#define E_ORS_OrderTraded (E_ORS_RMS_BASE + 57)
#define E_ORS_OrderCanceled (E_ORS_RMS_BASE + 58)
#define E_ORS_InvalidContract (E_ORS_RMS_BASE + 59)
#define E_ORS_InvalidDirection (E_ORS_RMS_BASE + 60)
#define E_ORS_NotFoundOrder (E_ORS_RMS_BASE + 61)
#define E_ORS_SendInsertFailed (E_ORS_RMS_BASE + 62)
#define E_ORS_SendCancelFailed (E_ORS_RMS_BASE + 63)
#define E_ORS_NoPositionToExec (E_ORS_RMS_BASE + 64)
#define E_ORS_ExchangeConnectionSendError (E_ORS_RMS_BASE + 65)

#define E_RMS_NO_ERROR E_NO_ORS_RMS_ERROR
#define E_RMS_BLOCKED_ACCOUNT (E_ORS_RMS_BASE + 100)
#define E_RMS_BLOCKED_SYMBOL (E_ORS_RMS_BASE + 101)
#define E_RMS_ORDER_QTY_LIMIT_BREACHED (E_ORS_RMS_BASE + 102)
#define E_RMS_ORDER_VALUE_LIMIT_BREACHED (E_ORS_RMS_BASE + 103)
#define E_RMS_UNEXPECTED_ORDERID (E_ORS_RMS_BASE + 104) /*! for all the cases when orderid is not in order cache */
#define E_RMS_TICKER_OO_LIMIT_BREACHED (E_ORS_RMS_BASE + 105)
#define E_RMS_TICKER_VALUE_LIMIT_BREACHED (E_ORS_RMS_BASE + 106)
#define E_RMS_ORDER_FREQUENCY_LIMIT_BREACHED (E_ORS_RMS_BASE + 107)
#define E_RMS_MODIFY_ORDER_NOT_FOUND (E_ORS_RMS_BASE + 108)
#define E_RMS_CANCEL_ORDER_NOT_FOUND (E_ORS_RMS_BASE + 109)
#define E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_BREACHED (E_ORS_RMS_BASE + 110)
#define E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_WARNING (E_ORS_RMS_BASE + 111)
#define E_RMS_INCOMPLETE_INIT (E_ORS_RMS_BASE + 112)
#define E_RMS_STRAT_WITH_MORE_ACCOUNTS (E_ORS_RMS_BASE + 113)
#define E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_WARNING (E_ORS_RMS_BASE + 114)
#define E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_BREACHED (E_ORS_RMS_BASE + 115)
#define E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING (E_ORS_RMS_BASE + 116)
#define E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_BREACHED (E_ORS_RMS_BASE + 117)
#define E_RMS_FIRM_LEVEL_STOP_LOSS_LIMIT_BREACHED (E_ORS_RMS_BASE + 118)
#define E_RMS_TOTAL_POSITION_IN_LOTS_FIRMLEVEL (E_ORS_RMS_BASE + 119)
#define E_RMS_QUANTITY_PERORDER_IN_LOTS_FIRMLEVEL (E_ORS_RMS_BASE + 120)
#define E_RMS_NOT_ALLOWED_SYMBOL (E_ORS_RMS_BASE + 121)
#define E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_WARNING (E_ORS_RMS_BASE + 122)
#define E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_BREACHED (E_ORS_RMS_BASE + 123)
#define E_RMS_MAX_CANCEL_CONFIRMS_LIMIT_BREACHED (E_ORS_RMS_BASE + 124)
#define E_RMS_TICKER_LEVEL_PERCENTAGE_AWAY_FROM_X_FAILED (E_ORS_RMS_BASE + 125)
#define E_RMS_ORDER_LEVEL_FILL_QUANTITY_MISMATCH (E_ORS_RMS_BASE + 126)
#define E_RMS_NET_POSITION_IN_LOTS_FIRMLEVEL (E_ORS_RMS_BASE + 127)
#define E_RMS_FOUND_NEGATIVE_PRICE (E_ORS_RMS_BASE + 128)
#define E_RMS_FOUND_NEGATIVE_QUANTITY (E_ORS_RMS_BASE + 129)

#define E_SANITY_BASE 1000000
#define E_SANITY_NO_ERROR (E_SANITY_BASE + 0)
#define E_SANITY_ORDERBOOK_CROSS (E_SANITY_BASE + 1)
#define E_SANITY_LTP_MISMATCH_NEWPRICE (E_SANITY_BASE + 2)
#define E_SANITY_LTQ_MISMATCH_NEWQTY (E_SANITY_BASE + 3)
#define E_SANITY_LTQ_MISMATCH_PREV_BEST_BID (E_SANITY_BASE + 4)
#define E_SANITY_LTP_MISMATCH_PREV_BEST_ASK (E_SANITY_BASE + 5)
#define E_SANITY_LTQ_MISMATCH_PREV_BEST_ASK (E_SANITY_BASE + 6)
#define E_SANITY_SEQUENCE_WRONG (E_SANITY_BASE + 7)
#define E_SANITY_RPT_SEQUENCE_WRONG (E_SANITY_BASE + 8)
#define E_SANITY_LEVEL_NEGATIVE_QTY (E_SANITY_BASE + 9)
#define E_SANITY_BID_ORDER_INVALID (E_SANITY_BASE + 10)
#define E_SANITY_ASK_ORDER_INVALID (E_SANITY_BASE + 11)
#define E_SANITY_ETS_INVALID (E_SANITY_BASE + 12)
#define E_SANITY_LTS_INVALID (E_SANITY_BASE + 13)
#define E_SANITY_LTP_MISMATCH_PREV_BEST_BID (E_SANITY_BASE + 14)
#define E_SANITY_VALIDBIDS_ASKS_BAD_VALUE (E_SANITY_BASE + 15)
#define E_SANITY_TTQ_ASC_ORDER_FAILED (E_SANITY_BASE + 16)
#define E_SANITY_ALL_MATCH_WITH_PREVIOUS_BOOK (E_SANITY_BASE + 17)
#define E_SANITY_SIDE_CHECK (E_SANITY_BASE + 18)
#define E_SANITY_VALID_BIDS_CHECK (E_SANITY_BASE + 19)
#define E_SANITY_VALID_ASKS_CHECK (E_SANITY_BASE + 20)

/*!
 * getErrorCodeDesciption
 * @ error_code
 * return const char*
 */
static const char *getErrorCodeDesciption(uint32_t error_code)
{
    switch (error_code)
    {
    case E_ORS_INVALID_SIDE:
        return "Invalid side passed to ORS server";
    case E_ORS_INVALID_ORDER_TYPE:
        return "Invalid order type passed to ORS server";
    case E_ORS_INVALID_DURATION:
        return "Invalid duration passed to ORS server";
    case E_ORS_INVALID_REQUEST_TYPE:
        return "Invalid request type passed to ORS server";
    case E_ORS_UNKNOWN_TRADING_SESSION:
        return "Unknown trading session, cannot process messages";
    case E_ORS_RECONNECTING_TO_EXCHANGE:
        return "ORS is reconnecting to exchange, retry again";
    case E_ORS_SESSION_TERMINATED:
        return "ORS session to exchange has been terminated(gracefully)";
    case E_ORS_SESSION_ABORTED:
        return "ORS session to exchange - ABORTED!!!";
    case E_ORS_SYMBOLMAPPING_NOT_FOUND:
        return "Symbol mapping not found in ORS";
    case E_ORS_NOT_CONFIRMED:
        return "Modify/Cancel on order which is not yet confirmed from exchange";
    case E_ORS_QUEUED_ORDER_REJECT:
        return "Auto Order Reject from FEP";
    case E_ORS_SELF_TRADE:
        return "Exchange cancelled order to prevent self trade";

    case E_ORS_NoPositionToClose:
        return "No Positon to close";
    case E_ORS_NoMoneyToOpen:
        return "No money to open";
    case E_ORS_InsertOrderTooFast:
        return "Insert order too fast";
    case E_ORS_OrderTypeNotSupported:
        return "Order type not supported";
    case E_ORS_OrderFinished:
    case E_ORS_OrderCanceled:
    case E_ORS_OrderTraded:
        return "Order finished";
    case E_ORS_PriceOutOfLimit:
        return "Price out of limit";
    case E_ORS_InvalidTradeSection:
        return "Not trade section";
    case E_ORS_InvalidContract:
        return "Invalid contract";
    case E_ORS_InvalidDirection:
        return "Invalid direction, not buy or sell";
    case E_ORS_NotFoundOrder:
        return "Not found order in cache";
    case E_ORS_SendInsertFailed:
        return "Call InsertOrder api failed";
    case E_ORS_SendCancelFailed:
        return "Call CancelOrder api failed";
    case E_ORS_NoPositionToExec:
        return "No long position for option exec";
    case E_ORS_ExchangeConnectionSendError:
        return "Insert order too fast Or Net disconnected";

    case E_RMS_BLOCKED_ACCOUNT:
        return "Invalid account used for trading!!!";
    case E_RMS_BLOCKED_SYMBOL:
        return "Invalid symbol used for trading!!!";
    case E_RMS_ORDER_QTY_LIMIT_BREACHED:
        return "Order Qty limit breached by the order";
    case E_RMS_ORDER_VALUE_LIMIT_BREACHED:
        return "Order value limit breached by the order";
    case E_RMS_TICKER_OO_LIMIT_BREACHED:
        return "Open Order limit breached on the ticker";
    case E_RMS_TICKER_VALUE_LIMIT_BREACHED:
        return "Exposure value breached on the ticker";
    case E_RMS_ORDER_FREQUENCY_LIMIT_BREACHED:
        return "Total Order Per Second Limit breached";
    case E_RMS_UNEXPECTED_ORDERID:
        return "UNEXPECTED order id. Possibly cache invalidation in ORS or strategy error";
    case E_RMS_MODIFY_ORDER_NOT_FOUND:
        return "Order Id of Modify Order NOT found in ORS cache";
    case E_RMS_CANCEL_ORDER_NOT_FOUND:
        return "Order Id of Cancel Order NOT found in ORS cache";
    case E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_BREACHED:
        return "Total Orders Per Day Limit Breached";
    case E_RMS_TOTAL_ORDERS_PER_DAY_LIMIT_WARNING:
        return "Total Orders Per Day Limit Warning";
    case E_RMS_INCOMPLETE_INIT:
        return "RMS Order Frequency Container is NOT initialized";
    case E_RMS_STRAT_WITH_MORE_ACCOUNTS:
        return "Strategy is running with more than one trading accounts";
    case E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_WARNING:
        return "Total Orders Per Trade Limit Warning";
    case E_RMS_TOTAL_ORDERS_PER_TRADE_LIMIT_BREACHED:
        return "Total Orders Per Trade Limit Breached";
    case E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_WARNING:
        return "Account Level Stop Loss Limit Warning";
    case E_RMS_ACCOUNT_LEVEL_STOP_LOSS_LIMIT_BREACHED:
        return "Account Level Stop Loss Limit Breached";
    case E_RMS_FIRM_LEVEL_STOP_LOSS_LIMIT_BREACHED:
        return "Firm Level Stop Loss Limit Breached";
    case E_RMS_TOTAL_POSITION_IN_LOTS_FIRMLEVEL:
        return "Total position in lots Breached at Firm level";
    case E_RMS_QUANTITY_PERORDER_IN_LOTS_FIRMLEVEL:
        return "Quantity per order in lots for an order Breached at Firm level";
    case E_RMS_NOT_ALLOWED_SYMBOL:
        return "Symbol not allowed in trading";
    case E_ORS_PAUSED:
        return "ORS is paused";
    case E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_WARNING:
        return "Ticker Level Stop Loss Limit Warning";
    case E_RMS_TICKER_LEVEL_STOP_LOSS_LIMIT_BREACHED:
        return "Ticker Level Stop Loss Limit Breached";
    case E_RMS_MAX_CANCEL_CONFIRMS_LIMIT_BREACHED:
        return "Max Cancel Confirmation Limit Breached On Ticker";
    case E_RMS_TICKER_LEVEL_PERCENTAGE_AWAY_FROM_X_FAILED:
        return "Percentage Away from X Failed";
    case E_RMS_ORDER_LEVEL_FILL_QUANTITY_MISMATCH:
        return "Fill quantity mismatch between rms and strategy";
    case E_RMS_NET_POSITION_IN_LOTS_FIRMLEVEL:
        return "Net position in lots(SellTraded - BuyTraded) breached at Firm level";
    case E_RMS_FOUND_NEGATIVE_PRICE:
        return "Price is negative in order";
    case E_RMS_FOUND_NEGATIVE_QUANTITY:
        return "Quantity is negative in order";
    case E_RMS_NO_ERROR:
        return "";

    case E_SANITY_NO_ERROR:
        return "";
    case E_SANITY_ORDERBOOK_CROSS:
        return "Order Book Cross";
    case E_SANITY_LTP_MISMATCH_NEWPRICE:
        return "Last Traded Price Does NOT Match NewPrice Field";
    case E_SANITY_LTQ_MISMATCH_NEWQTY:
        return "Last Traded Qty Does NOT Match NewQty Field";
    case E_SANITY_LTP_MISMATCH_PREV_BEST_BID:
        return "Last Traded Price Does NOT Match Previous Best Bid Price";
    case E_SANITY_LTQ_MISMATCH_PREV_BEST_BID:
        return "Last Traded Qty Does NOT Match Previous Best Bid Qty";
    case E_SANITY_LTP_MISMATCH_PREV_BEST_ASK:
        return "Last Traded Price Does NOT Match Previous Best Ask Price";
    case E_SANITY_LTQ_MISMATCH_PREV_BEST_ASK:
        return "Last Traded Qty Does NOT Match Previous Best Ask Quantity";
    case E_SANITY_SEQUENCE_WRONG:
        return "Sequence Numbers Are Out Of Order";
    case E_SANITY_RPT_SEQUENCE_WRONG:
        return "Repeat Sequence Numbers Are Out Of Order";
    case E_SANITY_LEVEL_NEGATIVE_QTY:
        return "Order Book Has Negative Qty At a Level";
    case E_SANITY_BID_ORDER_INVALID:
        return "Bid Prices Are NOT In Descending Order";
    case E_SANITY_ASK_ORDER_INVALID:
        return "Ask Prices Are NOT In Ascending Order";
    case E_SANITY_ETS_INVALID:
        return "Exchange Timestamp Is Invalid";
    case E_SANITY_LTS_INVALID:
        return "Local Timestamp Is Invalid";
    case E_SANITY_VALIDBIDS_ASKS_BAD_VALUE:
        return "Valid Ask and Bid levels is NOT correctly set";
    case E_SANITY_TTQ_ASC_ORDER_FAILED:
        return "Total traded quantity in current update is less than the previous one";
    case E_SANITY_ALL_MATCH_WITH_PREVIOUS_BOOK:
        return "Book Completely matches with the previous book";
    case E_SANITY_SIDE_CHECK:
        return "Side set in the current book does not match with the change";
    case E_SANITY_VALID_BIDS_CHECK:
        return "Valid Bid Price levels is NOT correctly set in book";
    case E_SANITY_VALID_ASKS_CHECK:
        return "Valid Ask Price levels is NOT correctly set in book";
    default:
        return "Error Code message not in <CommonUtils/include/errorcodes.h>";
    };
}

#endif

/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */

#ifndef RMSTEST_H
#define RMSTEST_H

#include "rmsimpl.h"
#include "rmstestbase.h"
#include <string>
#include <map>
/*!
 * The test is written with the following values.
 * ...
 *
    #########################
    #### GLOBAL SECTION #####
    #########################
    CHECK_LIST=VALID_SYMBOL_CHECK, ORDER_LIMIT_CHECK, TICKER_LIMIT_CHECK, ORDER_FREQUENCY_CHECK, ORDERS_PER_DAY_CHECK

    # banned symbols and order frequency limits. Frequency
    # window is in seconds
    BANNED_SYMBOLS=SBER123
    ORDER_FREQUENCY_LIMIT=300
    ORDER_FREQUENCY_WINDOW=1

    # Section name contains allowed accounts and their thresholds
    # orders for accounts not configured here, will be rejected by RMS
    [ PRP05, PRP06 ]
    SINGLE_ORDER_OPEN_QTY_THRESHOLD  = 4
    SINGLE_ORDER_OPEN_VALUE_THRESHOLD= 500
    PER_TICKER_OPEN_ORDERS_LIMIT     = 3
    PER_TICKER_NET_VALUE_LIMIT       = 1000
    CHECK_FOR_EXPOSURE               = TRUE
    # The following 5 fields is for calculation of current allowed orders
    # rms warning will be sent if % of total orders so far crosses lower limit
    # rms error will be sent if % of total orders so far crosses upper limit
    ORDERS_PER_DAY_FIXED_LIMIT   = 1000
    ORDER_TO_COMMISSION_FACTOR   = 1
    TRADED_VALUE_MULTIPLY_FACTOR = 1
    ORDERS_PER_DAY_LOWER_LIMIT   = 0.80
    ORDERS_PER_DAY_UPPER_LIMIT   = 0.99

    [ INFRA]
    SINGLE_ORDER_OPEN_QTY_THRESHOLD  = 3
    SINGLE_ORDER_OPEN_VALUE_THRESHOLD= 200
    PER_TICKER_OPEN_ORDERS_LIMIT     = 2
    PER_TICKER_NET_VALUE_LIMIT       = 300
    CHECK_FOR_EXPOSURE               = TRUE
    ORDERS_PER_DAY_FIXED_LIMIT       = 1000
    ORDER_TO_COMMISSION_FACTOR       = 20
    TRADED_VALUE_MULTIPLY_FACTOR     = 0.05
    ORDERS_PER_DAY_LOWER_LIMIT       = 0.90
    ORDERS_PER_DAY_UPPER_LIMIT       = 0.99
 */
class RMSTest : public RMSTestBase
{
public:
    RMSTest(std::string rmsConfig) : RMSTestBase(rmsConfig) {}
    void run();
    virtual ~RMSTest(){};
    virtual void instrumentInit();

private:
    void VALID_SYMBOL_CHECK();
    void ORDER_LIMIT_CHECK();
    void TICKER_LIMIT_CHECK();
    void ORDER_FREQUENCY_CHECK(int noOfRequests);
    void ORDERS_PER_DAY_CHECK(int noOfRequests);
};

#endif // RMSTEST_H

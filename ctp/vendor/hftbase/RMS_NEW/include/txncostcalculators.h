/*
 * txncostcalculators.h
 *
 *  Created on: Sep 3, 2013
 *      Author: varun
 */

#ifndef TXNCOSTCALCULATORS_H_
#define TXNCOSTCALCULATORS_H_

#include "netpositionaggregate_new.h"

namespace illuminati
{
    namespace infra
    {
        enum TxnCostCalculatorTypes
        {
            TXN_COST_ON_TRADED_VALUE = 1,
            TXN_COST_ON_TRADED_QTY,
            TXN_COST_ON_TRADED_QTY_AND_VALUE
        };

        class TxnCostCalculator
        {
        public:
            inline __attribute__((always_inline)) static double txnCostOnTradedValue(const TickerPositions &tickerPosition)
            {
                double txnBuyCost = (tickerPosition.netBuyTradedValue * tickerPosition.m_instrumentDetails.PriceFactor *
                                     tickerPosition.thresholds->m_buyTransactionCostFactorOnValue);

                double txnSellCost = (tickerPosition.netSellTradedValue * tickerPosition.m_instrumentDetails.PriceFactor *
                                      tickerPosition.thresholds->m_sellTransactionCostFactorOnValue);

                double totalTxnCost = txnBuyCost + txnSellCost;

                return totalTxnCost;
            }

            inline __attribute__((always_inline)) static double txnCostOnTradedQty(const TickerPositions &tickerPosition)
            {
                double txnBuyCost = tickerPosition.netBuyTradedQty * tickerPosition.thresholds->m_buyTransactionCostFactorOnQty;
                double txnSellCost = tickerPosition.netSellTradedQty * tickerPosition.thresholds->m_sellTransactionCostFactorOnQty;
                double totalTxnCost = txnBuyCost + txnSellCost;

                return totalTxnCost;
            }

            inline __attribute__((always_inline)) static double txnCostOnTradedQtyAndValue(const TickerPositions &tickerPosition)
            {
                double txnCostBasedOnValue = txnCostOnTradedValue(tickerPosition);
                double txnCostBasedOnQty = txnCostOnTradedQty(tickerPosition);

                double totalTxnCost = txnCostBasedOnQty + txnCostBasedOnValue;

                return totalTxnCost;
            }
        };
    }
}

#endif /* TXNCOSTCALCULATORS_H_ */

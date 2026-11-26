#ifndef _INFRA_ORDERBOOKROUTER_H
#define _INFRA_ORDERBOOKROUTER_H

#include "WriteFileUsingCstdio.h"
#include "marketupdateNew.h"
// #include "shmqueue.h"
#include "mdshmwriter.h"
#include "marketdataroutemodes.h"

namespace illuminati
{
    namespace infra
    {
        class MarketDataRouter
        {
        public:
            MarketDataRouter(size_t key = 0, size_t size = 0, MdRouteMode mode = ROUTE_FILE);
            ~MarketDataRouter();

            /*!
             * Method is added to make MarketDataRouter consistent with DataProcessor Policy
             */
            void finish()
            {
            }
            /*!
             * Method is added to make MarketDataRouter consistent with DataProcessor Policy
             * @param update orderbook data
             */
            void execDumpStrat(illuminati::md::MarketUpdateNew *update)
            {
                routeMdUpdate(update);
            }
            /*!
             * Method to route the orderbook data to FILE, SHM
             * @param update orderbook data that needs to be transferred
             */
            inline void routeMdUpdate(const illuminati::md::MarketUpdateNew *update)
            {
                if (m_mode == illuminati::infra::ROUTE_SHM)
                {
                    // single thread writing so not using locks
                    // m_writeToShm->enqueue(*update);
                    m_writeToShm.write(update);
                }
                else if (m_mode == illuminati::infra::ROUTE_FILE)
                {
                    cout << update->toString();
                    m_writeToFile.stream_write_all(update, sizeof(illuminati::md::MarketUpdateNew), 1);
                }
            }

        private:
            MdRouteMode m_mode;
            WriteFileUsingCstdio<1, void> m_writeToFile;
            // illuminati::ds::ShmCircularQueue<illuminati::md::MarketUpdateNew>* m_writeToShm;
            md::MarketDataShmWriter m_writeToShm;
        };
    }
}

#endif

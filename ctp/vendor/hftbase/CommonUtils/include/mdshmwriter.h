/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 */
#ifndef MARKETDATA_SHM_WRITER_
#define MARKETDATA_SHM_WRITER_

#include "marketupdateNew.h"
#include "shmmanager.h"
#include "configreader.h"

namespace illuminati
{
    namespace md
    {
        const int MAX_MD_CLIENTS = 100; // MAX writeres

        /*!
         * MarketDataShmWriter - Fixed INTEREST_LEVELS
         */
        class MarketDataShmWriter
        {
            typedef illuminati::ipc::ShmManager<illuminati::md::MarketUpdateNew, void *, void *, MAX_MD_CLIENTS> ShmMgr;
            ShmMgr m_shmManager;
            ShmMgr::MdShmQ *m_MarketDataQueue;

        public:
            void init(size_t shmMDKey, uint64_t shmSize)
            {
                m_MarketDataQueue = m_shmManager.createMarketDataClient(shmMDKey, shmSize);
            }
            MarketDataShmWriter() {}

            MarketDataShmWriter(illuminati::Configfile &cfg)
            {
                int64_t shmMDKey = cfg.getInt64_s("ShmDataWriter", "Md_Shm_Key");
                int64_t shmSize = cfg.getInt64_s("ShmDataWriter", "Md_Shm_Size");
                init(shmMDKey, shmSize);
            }

            ~MarketDataShmWriter()
            {
                m_shmManager.shutdown();
            }

            inline __attribute__((always_inline)) void write(const MarketUpdateNew *update)
            {
                m_MarketDataQueue->enqueue(*update);
            }

            inline __attribute__((always_inline)) void routeMdUpdate(const MarketUpdateNew *update)
            {
                m_MarketDataQueue->enqueue(*update);
            }

            void finish()
            {
            }
        };

        /*!
         * TMarketDataShmWriter - interestLevel
         * TODO: change this to template based
         */
        class MarketDataShmWriter_5
        {
            typedef illuminati::ipc::ShmManager<illuminati::md::TMarketUpdateNew<5>, void *, void *, MAX_MD_CLIENTS> TShmMgr;
            TShmMgr m_shmManager;
            TShmMgr::MdShmQ *m_MarketDataQueue;

        public:
            void init(size_t shmMDKey, uint64_t shmSize)
            {
                m_MarketDataQueue = m_shmManager.createMarketDataClient(shmMDKey, shmSize);
            }
            MarketDataShmWriter_5() {}

            MarketDataShmWriter_5(illuminati::Configfile &cfg)
            {
                int64_t shmMDKey = cfg.getInt64_s("ShmDataWriter", "Md_Shm_Key");
                int64_t shmSize = cfg.getInt64_s("ShmDataWriter", "Md_Shm_Size");
                init(shmMDKey, shmSize);
            }

            ~MarketDataShmWriter_5()
            {
                m_shmManager.shutdown();
            }

            inline __attribute__((always_inline)) void write(const illuminati::md::TMarketUpdateNew<5> *update)
            {
                m_MarketDataQueue->enqueue(*update);
            }

            inline __attribute__((always_inline)) void routeMdUpdate(const illuminati::md::TMarketUpdateNew<5> *update)
            {
                m_MarketDataQueue->enqueue(*update);
            }

            void finish()
            {
            }
        };

    } // md
} // illuminati

#endif

/*
 * ShmAdditionalDataReader.h
 *
 *  Created on: 12-Sep-2013
 *      Author: neerajn
 */

#ifndef SHMADDITIONALDATAREADER_H_
#define SHMADDITIONALDATAREADER_H_

#include <fstream>
#include <thread>
#include <signal.h>

#include "shmmanager.h"
#include "marketupdateNew.h"
#include "marketdeltaNew.h"
#include "signalcallback.h"
#include "configreader.h"

namespace illuminati
{
    namespace infra
    {

#define MAX_SHM_TO_READ 10

        class ShmAdditionalDataDumper
        {

            typedef ipc::ShmManager<md::MarketUpdateNew, void *, void *,
                                    MAX_SHM_TO_READ>
                SHMManager;

        public:
            ShmAdditionalDataDumper(std::string &configFile);
            ~ShmAdditionalDataDumper();

            void start();
            void stop();

        private:
            void init(std::string &configFile);
            void processMD(md::MarketUpdateNew *md);

            SHMManager m_shmmanager;
            std::ofstream m_outMDFile;
            std::ofstream m_outADFile;
            std::atomic<int> m_stop;
            bool m_dumpMDAlso;

            AdditionalData m_additionalDataBuffer;
        };
    }
} /* namespace illuminati */
#endif /* SHMADDITIONALDATAREADER_H_ */

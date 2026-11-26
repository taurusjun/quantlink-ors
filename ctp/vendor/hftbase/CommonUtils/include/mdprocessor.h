/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef MDPROCESSOR_H
#define MDPROCESSOR_H

#include "marketupdateNew.h"
#include "signalcallback.h"
#include "SHMBasedStoragePolicy.h"
#include "constants.h"
#include "shmqueue.h"
#include "commontypes.h"
#include <iostream>
#include "itimer.h"

namespace illuminati
{
    namespace infra
    {
        template <typename OBInterface, typename Decoder, typename Manager>
        class MDProcessor
        {
            typedef MDProcessor<OBInterface, Decoder, Manager> mdp;

        public:
            DEFINE_SIGNAL(MarketUpdateSignal)
            // Argument specifies the total number of signals that can  be emitted by source
            SIGNAL_INIT(1)
        private:
            Decoder *m_decoder;
            Manager *m_manager;
            /* Q is made pointer so that it wont allocate shared mry if MDProcessor is used for dump pre-processing. */
            MdShmQueueWriter *mdContainerWriter;

        public:
#ifdef DUMP_MU
            std::ofstream dump;
#endif

            MDProcessor<OBInterface, Decoder, Manager>()
            {
#ifdef DUMP_MU
                dump.open("/home/Jijith/shm_write_dump", std::ios::binary | std::ios::trunc);
#endif
            }
            ~MDProcessor<OBInterface, Decoder, Manager>()
            {
#ifdef DUMP_MU
                dump.close();
#endif
            }
            void FeedMe(const char *buffer)
            {
                m_decoder->DecodeMessage(buffer);
            }

            void enableLiveMarketUpdateHandler()
            {
                mdContainerWriter = new MdShmQueueWriter();
                illuminati::md::MarketUpdateNew dummy;
                connectWithParamRef(this, this, &mdp::liveMarketUpdateHandler, mdp::MarketUpdateSignal, dummy);
            }

            void liveMarketUpdateHandler(illuminati::md::MarketUpdateNew &update)
            {
                update.m_exchangeName = illuminati::md::MICEX_FOND;
                update.m_feedType = illuminati::md::FEED_TBT;
                // write to shared memory
                /* update.print(); */
                mdContainerWriter->enqueue(update);
#ifdef DUMP_MU
                dump.write((const char *)&update, sizeof(illuminati::md::MarketUpdateNew));
#endif
            }

            void ProcessDecoded(OBInterface &obi)
            {
                std::vector<illuminati::md::MarketUpdateNew> updateVec;
                if (m_manager->ProcessFeed(obi, updateVec))
                {
                    for (auto updateIt = updateVec.begin(); updateIt != updateVec.end(); ++updateIt)
                    {
                        EMIT_PARAM(MarketUpdateSignal, *(updateIt));
                    }
                }
            }
            void SetDecoder(Decoder *m_decoder)
            {
                this->m_decoder = m_decoder;
                // connect 'DecodeSignal' thrown by m_decoder to this->ProcessDecoded. OBInterface is the param to the callback.
                OBInterface dummy;
                connectWithParam(this->m_decoder, this, &mdp::ProcessDecoded, Decoder::DecodeSignal, dummy);
            }
            void SetManager(Manager *m_manager)
            {
                this->m_manager = m_manager;
            }
        };

#define SimpleMDProcessor(Decoder) MDProcessor<illuminati::md::MarketUpdateNew, Decoder, Decoder>

        template <typename Decoder>
        class MDProcessor<illuminati::md::MarketUpdateNew, Decoder, Decoder>
        {
            typedef MDProcessor<illuminati::md::MarketUpdateNew, Decoder, Decoder> smdp;

        public:
            DEFINE_SIGNAL(MarketUpdateSignal)
            // Argument specifies the total number of signals that can  be emitted by source
            SIGNAL_INIT(1)
        private:
            Decoder *m_decoder;

        public:
            void FeedMe(const char *buffer, size_t size)
            {
                m_decoder->DecodeMessage(buffer, size);
            }
            void ProcessDecoded(illuminati::md::MarketUpdateNew *mu)
            {
                EMIT_PARAM(MarketUpdateSignal, mu);
            }
            void SetDecoder(Decoder *m_decoder)
            {
                this->m_decoder = m_decoder;
                // connect 'DecodeSignal' thrown by m_decoder to this->ProcessDecoded. OBInterface is the param to the callback.
                illuminati::md::MarketUpdateNew *dummy;
                connectWithParam(this->m_decoder, this, &smdp::ProcessDecoded, Decoder::DecodeSignal, dummy);
            }
        };

    }
}

#endif // MDPROCESSOR_H

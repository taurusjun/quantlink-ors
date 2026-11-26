/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef FEEDSEQUENCER_H
#define FEEDSEQUENCER_H

#include <stdint.h>
#include <thread>
#include <vector>
#include "c11compatible.h"

namespace illuminati
{
    namespace md
    {
        template <typename T, typename W>
        struct SequencerData
        {
            T *reader;
            W *writer;
            SequencerData(T *reader_in, W *writer_in) : reader(reader_in), writer(writer_in) {}
        };

        template <typename T>
        struct DataFormat
        {
            int index;
            T data;
            DataFormat()
            {
                index = 0;
            }
            DataFormat(int index, T data)
            {
                this->index = index;
                this->data = data;
            }
            DataFormat(int index, T *data)
            {
                this->index = index;
                this->data = *data;
            }
        };

        /*!
         * @tparam T type of the reader.
         * @tparam W type of the writer.
         * @tparam U Type of the resultant sequenced feed.
         * @tparam ComparisonFunc Function which is used for comparison.
         * Will place @c b before @c a if returned false.
         * Sequencing is done based on this function.
         */
        template <typename T, typename W, typename U, typename V, typename ComparisonFunc>
        class FeedSequencer
        {
        private:
            std::vector<SequencerData<T, W> *> readerList;
            int currentReaderListIndex;
            std::thread sequencerThread;
            U *sequencedFeed;
            bool babort;
            ComparisonFunc comparator;
            bool done;
            bool bfinish;

        public:
            FeedSequencer() : sequencedFeed(nullptr), babort(false), done(false), bfinish(false), currentReaderListIndex(0) {}
            ~FeedSequencer()
            {
                for (int i = 0; i < currentReaderListIndex; i++)
                    delete readerList[i];
            }

            bool addFeeder(T *feeder, W *writer);
            void removeAllFeeders();
            void setSequencedFeed(U *sequencedFeed)
            {
                this->sequencedFeed = sequencedFeed;
            }

            void startAsync();

            void start(U *sequencedFeed);
            void startAsync(U *sequencedFeed);
            bool hasCompleted()
            {
                return done;
            }
            void abort()
            {
                babort = true;
            }
            void stop()
            {
                bfinish = true;
            }
            void waitForCompletion()
            {
                if (sequencerThread.joinable())
                    sequencerThread.join();
            }
        };

    }
}

#endif // FEEDSEQUENCER_H

/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef FEEDSEQUENCER_CPP
#define FEEDSEQUENCER_CPP

#include "feedsequencer.h"
#include <iostream>
#include <thread>
#include "itimer.h"
#include "fastbinheap.hpp"
// #include "binaryheap.hpp"
// #include "processsettings.h"

using namespace illuminati::md;
using namespace illuminati::ds;

template <typename T, typename W, typename U, typename V, typename ComparisonFunc>
bool FeedSequencer<T, W, U, V, ComparisonFunc>::addFeeder(T *feeder, W *writer)
{
    readerList.push_back(new SequencerData<T, W>(feeder, writer));
    currentReaderListIndex++;
    return true;
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc>
void FeedSequencer<T, W, U, V, ComparisonFunc>::start(U *sequencedFeed)
{

    for (int index = 0; index < currentReaderListIndex; index++)
    {
        if (readerList[index]->reader == nullptr || readerList[index]->writer == nullptr)
        {
            std::cerr << "Reader is not initialized :: " << index << "\n";
            throw "Reader is not initialized";
        }
    }
    done = false;

#ifdef EXCHSIMSTATS
    uint64_t fullWaitTime = 0;
    uint64_t fullWaitTimeStart = 0;
    uint64_t emptyWaitTime = 0;
    uint64_t emptyWaitTimeStart = 0;
    uint64_t enqueueTime = 0;
    uint64_t enqueueTimeStart = 0;
#endif

#ifdef EXCHSIMSTATS
    uint64_t heapTime = 0;
    uint64_t heapTimeStart = 0;
#endif

    FastBinHeap<DataFormat<V>, ComparisonFunc> heap(currentReaderListIndex);
    for (int i = 0; i < currentReaderListIndex; i++)
    {
        while (readerList[i]->reader->isEmpty() && !readerList[i]->writer->hasCompleted())
        {
            usleep(5);
        }
        if (!readerList[i]->reader->isEmpty())
        {
            heap.insert(DataFormat<V>(i, readerList[i]->reader->dequeue()));
        }
    }

    V d;
    while (!babort)
    {
        int bestIndex = -1;
        bool allEmpty = true;
        bool waitForWriter = false;

        while (!heap.isEmpty())
        {
            DataFormat<V> *data = heap.getRootPtr();

#ifdef EXCHSIMSTATS
            fullWaitTimeStart = ITime_RDTSC::GetCurrentTime();
#endif
            while (sequencedFeed->isFull() && !babort)
            {
                usleep(5);
            }
#ifdef EXCHSIMSTATS
            fullWaitTime += (ITime_RDTSC::GetCurrentTime() - fullWaitTimeStart);
            enqueueTimeStart = ITime_RDTSC::GetCurrentTime();
#endif
            if (babort)
                break;
            sequencedFeed->enqueue(data->data);

#ifdef EXCHSIMSTATS
            enqueueTime += (ITime_RDTSC::GetCurrentTime() - enqueueTimeStart);
            emptyWaitTimeStart = ITime_ClockRT::GetCurrentTime();
#endif
            while (readerList[data->index]->reader->isEmpty() && !readerList[data->index]->writer->hasCompleted())
            {

                usleep(5);
            }
#ifdef EXCHSIMSTATS
            emptyWaitTime += ITime_ClockRT::GetCurrentTime() - emptyWaitTimeStart;
#endif

            if (!readerList[data->index]->reader->isEmpty())
            {

#ifdef EXCHSIMSTATS
                heapTimeStart = ITime_ClockRT::GetCurrentTime();
#endif

                V *dptr = readerList[data->index]->reader->getDequeuePtr();
                heap.removeRootAndInsert(DataFormat<V>(data->index, dptr));
                readerList[data->index]->reader->commitDequeue();

#ifdef EXCHSIMSTATS
                heapTime += ITime_ClockRT::GetCurrentTime() - heapTimeStart;
#endif
            }
            else
            {
                heap.removeRoot();
            }
        }
        break;
    }

#ifdef EXCHSIMSTATS
    double total = enqueueTime + heapTime + fullWaitTime + emptyWaitTime;
    std::cout.imbue(std::locale("C"));
    std::cout << "Sequencer thread:\n";
    std::cout << " Total enqueueTime  : " << enqueueTime << " " << ((double)enqueueTime) / total * 100 << "\n";
    std::cout << " Total deque+insert : " << heapTime << " " << ((double)heapTime) / total * 100 << "\n";
    std::cout << " Total fullWaitTime : " << fullWaitTime << " " << ((double)fullWaitTime) / total * 100 << "\n";
    std::cout << " Total emptyWaitTime: " << emptyWaitTime << " " << ((double)emptyWaitTime) / total * 100 << "\n";
#endif

    done = true;
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc>
void FeedSequencer<T, W, U, V, ComparisonFunc>::removeAllFeeders()
{
    for (int index = 0; index < currentReaderListIndex; index++)
    {
        if (readerList[index]->reader != nullptr)
        {
            delete readerList[index]->reader;
        }
    }
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc>
void FeedSequencer<T, W, U, V, ComparisonFunc>::startAsync()
{
    this->startAsync(this->sequencedFeed);
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc>
void FeedSequencer<T, W, U, V, ComparisonFunc>::startAsync(U *sequencedFeed)
{
    babort = false;
    sequencerThread = std::thread(&FeedSequencer<T, W, U, V, ComparisonFunc>::start, this, sequencedFeed);
}

#endif

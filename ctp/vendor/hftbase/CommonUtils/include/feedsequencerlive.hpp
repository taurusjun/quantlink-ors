/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef FEEDSEQUENCERLIVE_HPP
#define FEEDSEQUENCERLIVE_HPP

#include "feedsequencerlive.h"
#include <iostream>
#include <thread>
#include "itimer.h"
#include "binaryheap.hpp"
#include "logger.h"
#include "processsettings.h"

using namespace illuminati::md;
using namespace illuminati::ds;

template <typename T, typename W, typename U, typename V, typename ComparisonFunc, typename Wrapper>
const std::string FeedSequencerLive<T, W, U, V, ComparisonFunc, Wrapper>::FEED_SEQUENCER = "FEED_SEQUENCER";

template <typename T, typename W, typename U, typename V, typename ComparisonFunc, typename Wrapper>
bool FeedSequencerLive<T, W, U, V, ComparisonFunc, Wrapper>::addFeeder(T *feeder, W *writer)
{
    readerList.push_back(new SequencerDataLive<T, W>(feeder, writer));
    currentReaderListIndex++;
    return true;
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc, typename Wrapper>
void FeedSequencerLive<T, W, U, V, ComparisonFunc, Wrapper>::start()
{
    illuminati::sysinfo::ProcessSettings ps;
    ps.configure(FEED_SEQUENCER);

    // input validation.
    for (int index = 0; index < currentReaderListIndex; index++)
    {
        if (readerList[index]->reader == nullptr || readerList[index]->writer == nullptr)
        {
            std::cerr << "Reader is not initialized :: " << index << "\n";
            throw "Reader is not initialized";
        }
    }
    done = false;
    uint64_t totalPackets = 0;
    ITime_HighResolution loopTimer, completeTimer;
    uint64_t totalLoopTime = 0, totalLoopIts = 0;

    BinaryHeap<Wrapper, ComparisonFunc> heap;

    int bestIndex = -1;
    bool allEmpty = true;
    bool waitForWriter = false;

    while (!babort)
    {

        bestIndex = -1;
        allEmpty = true;
        waitForWriter = false;

        // Now heap is empty, so pick an element from each queue, if available
        while (heap.isEmpty())
        {
            for (int i = 0; i < currentReaderListIndex; i++)
            {
                if (!readerList[i]->reader->isEmpty())
                {
                    Wrapper *temp = readerList[i]->reader->dequeuePtr();
                    temp->index = i;
                    heap.insert(*temp);
                }
            }
        }

        while (!heap.isEmpty())
        {
            Wrapper data = heap.getRoot();
            while (sequencedFeed->isFull() && !babort)
            {
                // ILOG(INFO)<<"Sequenced field is full: waiting ...\n";
                // std::this_thread::yield();
                // usleep(5);
            }
            // Try adding false branch prediction indicator here
            if (babort)
                break;
            sequencedFeed->enqueue(data);

            if (!readerList[data.index]->reader->isEmpty())
            {
                Wrapper *temp = readerList[data.index]->reader->dequeuePtr();
                temp->index = data.index;
                heap.removeRootAndInsert(*temp);
            }
            else
            {
                heap.removeRoot();
            }
        }
    }

    done = true;
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc, typename Wrapper>
void FeedSequencerLive<T, W, U, V, ComparisonFunc, Wrapper>::removeAllFeeders()
{
    for (int index = 0; index < currentReaderListIndex; index++)
    {
        if (readerList[index]->reader != nullptr)
        {
            delete readerList[index]->reader;
        }
    }
}

template <typename T, typename W, typename U, typename V, typename ComparisonFunc, typename Wrapper>
void FeedSequencerLive<T, W, U, V, ComparisonFunc, Wrapper>::startAsync()
{
    babort = false;
    sequencerThread = std::thread(&FeedSequencerLive<T, W, U, V, ComparisonFunc, Wrapper>::start, this);
}

#endif

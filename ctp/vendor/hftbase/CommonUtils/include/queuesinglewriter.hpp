/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef __QUEUE_SINGLE_WRITER_HPP__
#define __QUEUE_SINGLE_WRITER_HPP__

#include "queuesinglewriter.h"
#include <iostream>

using namespace illuminati::ds;

template <class T, typename QueueBaseType>
void QueueSingleWriter<T, QueueBaseType>::enqueue(T value)
{
    // For now, we will be overwriting even if reader has not read. However reader can recognize this.
    int64_t localHead = queue->head.load();

    // We assume that buffer is always pre-allocated.
    //  queue->load(localHead)->value = value;
    //  queue->load(localHead)->writeCount++;
    queue->store(localHead, value);       // value will be converted to QueueNode on the fly.
    queue->load(localHead)->writeCount++; // Is this really necessary?(Do we need to detect overwrite?)

    queue->addOne(localHead);
    queue->head.store(localHead);
}

template <class T, typename QueueBaseType>
bool QueueSingleWriter<T, QueueBaseType>::isFull()
{
    int64_t localHead = queue->head.load();
    int64_t localTail = queue->tail.load();
    queue->addOne(localHead);
    // std::cout<<"head="<<localHead<<" tail="<<localTail<<"\n";
    if (localHead == localTail)
        return true;
    return false;
}

template <class T, typename QueueBaseType>
int64_t QueueSingleWriter<T, QueueBaseType>::getRemainingSize()
{
    int64_t localHead = queue->head.load();
    int64_t localTail = queue->tail.load();
    // std::cout<<"localHead="<<localHead<<" localTail="<<localTail<<"\n";
    if (localHead >= localTail)
        return (queue->getMaxSize() - localHead + localTail - 1);
    else
        return localTail - localHead - 1;
}

#endif

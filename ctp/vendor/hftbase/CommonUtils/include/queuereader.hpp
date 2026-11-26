/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef __QUEUE_SINGLE_READER_HPP__
#define __QUEUE_SINGLE_READER_HPP__

#include "queuereader.h"
#include "itimer.h"
#include <thread>
#include <iostream>

using namespace illuminati::ds;

template <class T, typename QueueBaseType>
T QueueReader<T, QueueBaseType>::dequeue()
{
    T value = queue->load(localTail)->value;
    localTail = (localTail + 1) % queue->m_size;
    queue->tail.store(localTail);
    return value;
}

template <class T, typename QueueBaseType>
T *QueueReader<T, QueueBaseType>::dequeuePtr()
{
    auto node = queue->load(localTail);
    localTail = (localTail + 1) % queue->m_size;
    queue->tail.store(localTail);
    return &(node->value);
}

template <class T, typename QueueBaseType>
bool QueueReader<T, QueueBaseType>::dequeueWaitToFill(T *value, int waitMilliSec)
{
    int64_t elapsedTime = 0;
    ITime_HighResolution timer;

    while (elapsedTime < waitMilliSec && isEmpty())
    {
        timer.start();
        std::this_thread::yield();
        timer.stop();
        elapsedTime += timer.getelapsedMillSecs();
    }
    if (elapsedTime == waitMilliSec)
        return false;

    *value = dequeue();

    return true;
}

template <class T, typename QueueBaseType>
bool QueueReader<T, QueueBaseType>::hasOverwritten()
{
    std::cout << "hasOverwritten():: localWriteCount=" << localWriteCount << " writecount=" << queue->load(localTail)->writeCount << "\n";
    return localWriteCount != queue->load(localTail)->writeCount;
}

template <class T, typename QueueBaseType>
T QueueReader<T, QueueBaseType>::dequeueWithOverwriteCheck(bool &bOverwrite)
{
    if (hasOverwritten())
    {
        bOverwrite = true;
        return false;
    }
    bOverwrite = false;
    T value = dequeue();
    return value;
}

template <class T, typename QueueBaseType>
bool inline QueueReader<T, QueueBaseType>::isEmpty()
{
    return localTail == queue->head.load();
}

template <class T, typename QueueBaseType>
T QueueReader<T, QueueBaseType>::front()
{
    return queue->load(localTail)->value;
}

/* Since the circular queue is a queue of "QueueNode" structures,
   inorder to omit reading the data member of type "long" from the structure, everytime sizeof(long) is added to the source. */

template <class T, typename QueueBaseType>
int QueueReader<T, QueueBaseType>::copyTillCurrentHead(T *dest)
{
    int totNum = 0;
    long currentHead = (queue->head.load());
    if (localTail != currentHead)
    {
        size_t incSiz = 0;
        if (localTail < currentHead)
        {
            char *source = (char *)queue->load(localTail);
            for (int j = localTail; j < currentHead; j++)
            {
                source = source + sizeof(long);
                memcpy(dest + incSiz, source, sizeof(T));
                incSiz++;
                source = source + sizeof(T);
            }
            totNum = currentHead - localTail;
        }
        else
        {
            char *source = (char *)queue->load(localTail);
            for (int j = localTail; j < queue->m_size; j++)
            {
                source = source + sizeof(long);
                memcpy(dest + incSiz, source, sizeof(T));
                incSiz++;
                source = source + sizeof(T);
            }
            source = (char *)queue->load(0);
            for (int j = 0; j < currentHead; j++)
            {
                source = source + sizeof(long);
                memcpy(dest + incSiz, source, sizeof(T));
                incSiz++;
                source = source + sizeof(T);
            }
            totNum = queue->m_size - localTail + currentHead;
        }
        localTail = currentHead;
        queue->tail.store(localTail);
    }
    return totNum;
}

template <class T, typename QueueBaseType>
void QueueReader<T, QueueBaseType>::reset()
{
    localTail = (queue->head.load());
    queue->tail.store(localTail);
}
#endif

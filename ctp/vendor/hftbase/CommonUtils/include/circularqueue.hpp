/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef __CIRCULAR_QUEUE_HPP
#define __CIRCULAR_QUEUE_HPP

#include "circularqueue.h"

using namespace illuminati::ds;

template <class T, template <typename...> class Storage, typename... Args>
CircularQueue<T, Storage, Args...>::CircularQueue()
{
    head = 0;
    tail = 0;
    writerCount.store(0);
    readerCount.store(0);
}

template <class T, template <typename...> class Storage, typename... Args>
QueueNode<T> *CircularQueue<T, Storage, Args...>::load(int index)
{
    return Storage<QueueNode<T>, Args...>::at(index);
}

// template<class T, template<typename...> class Storage, typename... Args>
// void CircularQueue<T, Storage, Args...>::store ( int index, QueueNode< T >* value )
// {
//   Storage<QueueNode<T>, Args...>::store(index, value);
// }

template <class T, template <typename...> class Storage, typename... Args>
void CircularQueue<T, Storage, Args...>::addOne(int64_t &index)
{
    index = (index + 1) % (Storage<QueueNode<T>, Args...>::m_size);
}

#endif

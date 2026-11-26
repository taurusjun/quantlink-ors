/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#include "queuefactory.h"

using namespace illuminati::ds;

template <class T, class Storage>
QueueFactory<T, Storage>::QueueFactory()
{
    circularQueue = nullptr;
}

template <class T, class Storage>
QueueFactory<T, Storage>::~QueueFactory()
{
    delete circularQueue;
}
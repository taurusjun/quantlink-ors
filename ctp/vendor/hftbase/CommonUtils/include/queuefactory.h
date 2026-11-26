/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef QUEUEFACTORY_H
#define QUEUEFACTORY_H

#include "circularqueuebase.cpp"
#include "circularqueuereader.h"
#include "circularqueuesinglewriter.h"
#include <iostream>

namespace illuminati
{
    namespace ds
    {

        template <class T, class Storage>
        class QueueFactory
        {
        public:
            CircularQ(T, Storage) * circularQueue;
            QueueFactory();
            ~QueueFactory();
            static QueueFactory<T, Storage> *createCircularQueueFactory()
            {
                QueueFactory<T, Storage> *factory = new QueueFactory<T, Storage>();
                factory->circularQueue = new CircularQ(T, Storage)();
                return factory;
            }

            CircularQReader(T, Storage, SingleWriterSingleReader) * createReader()
            {
                CircularQReader(T, Storage, SingleWriterSingleReader) *reader = new CircularQReader(T, Storage, SingleWriterSingleReader)(circularQueue);
                // std::cout<<"factory(): creating reader: current head = "<<circularQueue->head<<"\n";
                return reader;
            }

            CircularQWriter(T, Storage, SingleWriterSingleReader) * createSingleWriter()
            {
                CircularQWriter(T, Storage, SingleWriterSingleReader) *writer = new CircularQWriter(T, Storage, SingleWriterSingleReader)(circularQueue);
                return writer;
            }
        };
    }
}

#endif // QUEUEFACTORY_H

/*
 * Company: way2wealth-ors
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-ors.com/
 *
 */

#ifndef ILLUMIANT_DS_QUEUEREADER_H
#define ILLUMIANT_DS_QUEUEREADER_H

namespace ors
{
    namespace ds
    {
        /*!
         * Gives a read only view of the queue.
         * This writer can be used for writing to any queue, which follows these requirements:
         * 1. Should have @c load() method which returns a @c struct which has @c value and @c writeCount members @sa QueueNode
         * 2. Should have @c addOne() method which increments the index by 1.
         * 3. @c tail and @c head members which are @c atomic_long for pointing to the slowest tail and current head.
         * @tparam T Type of the data in the queue
         * @tparam QueueBaseType Type of the queue
         */
        template <typename T, class QueueBaseType>
        class QueueReader
        {
        private:
            QueueBaseType *queue;
            long localTail;
            long localWriteCount;

        public:
            QueueReader(QueueBaseType *queue_in) : queue(queue_in), localTail(0), localWriteCount(1) {}

            /*!
             * Gets the front item from the queue and moves the tail pointer ahead.
             */
            T dequeue();
            T *dequeuePtr();

            /*!
             * Waits if the reader is ahead of writer. Calls @c std::this_thread::yield() while waiting to yield CPU.
             * @param [out] value Contains the front of the queue.
             * @param [in] waitMilliSec Milliseconds to wait if the queue is empty.
             */
            bool dequeueWaitToFill(T *value, int waitMilliSec = 10000);

            /*!
             * Dequeue along with an overwrite check.
             * @param [out] bOverwrite sets to true if overwritten.
             */
            T dequeueWithOverwriteCheck(bool &bOverwrite);

            /*!
             * Check if the current @c front was overwritten before it was read.
             */
            bool hasOverwritten();

            bool isEmpty();
            T front();
            int copyTillCurrentHead(T *dest);
            void reset();

            /**
             * Get the size of the queue as seen by this reader.
             */
            long getSize()
            {
                long size = 0;
                long currentHead = (queue->head.load());
                if (localTail <= currentHead)
                {
                    size = currentHead - localTail;
                }
                else
                {
                    size = currentHead + (queue->m_size - localTail);
                }
                return size;
            }
        };

    }
}
#endif // QUEUEREADER_H

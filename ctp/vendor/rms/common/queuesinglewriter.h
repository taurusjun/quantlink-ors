/*
 * Company: way2wealth-ors
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-ors.com/
 *
 */

#ifndef ILLUMINATI_DS_QUEUEWRITER_H
#define ILLUMINATI_DS_QUEUEWRITER_H

namespace ors
{
    namespace ds
    {
        /*!
         * Gives a write only view of the queue.
         * This writer can be used for writing to any queue, which follows these requirements:
         * 1. Should have @c load() method which returns a @c struct which has @c value and @c writeCount members @sa QueueNode
         * 2. Should have @c addOne() method which increments the index by 1.
         * 3. @c tail and @c head members which are @c atomic_long for pointing to the slowest tail and current head.
         * @tparam T Type of the data in the queue
         * @tparam QueueBaseType Type of the queue
         */
        template <typename T, typename QueueBaseType>
        class QueueSingleWriter
        {
        private:
            QueueBaseType *queue;
            long writerHead;

        public:
            QueueSingleWriter(QueueBaseType *queue_in) : queue(queue_in)
            {
                // We will write into the index where writerHead points to and increment it.
                // Thus writerHead is pointing to a vacant location(can get overwritten next time).
                writerHead = 0;
            }
            void enqueue(T);

            /**
             * Only use this function if you have single writer and single reader.
             */
            bool isFull();

            /**
             * Only use this function if you have single writer and single reader.
             */
            int64_t getRemainingSize();
        };
    }
}

#endif // CIRCULARQUEUEWRITER_H

/*
 * Company: way2wealth-ors
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-ors.com/
 *
 */

#ifndef ILLUMINATI_DS_CIRCULAR_QUEUE_H
#define ILLUMINATI_DS_CIRCULAR_QUEUE_H

#include "c11compatible.h"
#include <vector>
#include <stdint.h>
#include <stddef.h>

#include "queuereader.h"
#include "queuesinglewriter.h"

#define INT(i) std::integral_constant<int, i>
/*!
 * Circular queue which supports multiple reads or writes without any locking.
 */
#define CircularQMultiAccess(T, S, ...) ::ors::ds::CircularQueue<T, S, __VA_ARGS__>

/*!
 * Normal Circular queue. If compiler supports template alias with variads, this macro should be replaced by following
 *
 * typedef<typename T, typename S, typename... Args>
 * using CircularQ = CircularQueue<T, S, bool, Args ...>
 */
#define CircularQ(T, S, ...) ::ors::ds::SimpleCircularQueue<T, S, __VA_ARGS__>
/*!
 * Singlewriter and Multiwriter circular queue.
 * Mutliwriter queue is not yet implemented.
 */
#define CircularQSW(T, S, ...) ::ors::ds::QueueSingleWriter<T, ::ors::ds::CircularQueue<T, S, __VA_ARGS__>>
// #define CircularQMW(T, S, ...) CircularQueueSingleWriter<T, CircularQueue<T, S, true, __VA_ARGS__>>

/*!
 * Circular queue reader which can have single or multiple access to the queue.
 * To have multiple access create more instances of this reader based on the same queue.
 */
#define CircularQR(T, S, ...) ::ors::ds::QueueReader<T, ::ors::ds::CircularQueue<T, S, __VA_ARGS__>>

/*!
 * This macro is only used in the CircularQueue implementation, should not be used anywhere outside.
 * Should replace this macro with template alias if GCC >4.7 can be used.
 */
#define _CircularQSWType_(T, S, AL) ::ors::ds::QueueSingleWriter<T, ::ors::ds::CircularQueue<T, S, Args...>>
/*!
 * This macro is only used in the CircularQueue implementation, should not be used anywhere outside.
 * Should replace this macro with template alias if GCC >4.7 can be used.
 */
#define _CircularQReaderType_(T, S, AL) ::ors::ds::QueueReader<T, ::ors::ds::CircularQueue<T, S, Args...>>

namespace ors
{
    namespace ds
    {
        template <class T>
        struct QueueNode
        {
            long writeCount; // Number of times this node is overwritten. It can be used for identifying if a node is overwritten before it getting read.
            T value;
            // char padding[ ( ((( sizeof(T)*8 + 64 )/128) + 1)*128) - ( sizeof(T)*8 + 64 ) ];
            // char padding[64];

            QueueNode() : writeCount(0)
            {
            }
            QueueNode(T value_in) : writeCount(0), value(value_in)
            {
            }
        };

        /*!\brief Lockless Circular queue, which supports concurrent read or write.
         *
         * Atomic integers and fetch-add, modify-write operations are used for lock-free synchronization.
         * Allows to specify storage policy as a template parameter. Following are the requirements of the storage policy
         * 1. Can have any number of template parameters.
         * 2. First parameter should always be the type of the data to be stored.
         * 3. Compile time constants should be wrapped using std::integral_constant.
         * 4. Should provide method @c at with the signature
         * @code
         * T* at(int64_t pos)
         * @endcode
         * 5. Should provide member @c m_size of type int64_t
         *
         * @tparam T Type of data to be stored
         * @tparam Storage Storage policy.
         * @tparam isMultiAccess True if the queue supports concurrent access.
         * This is used to provide a partial specificiation for queues without any concurrent support.
         * @tparam Args Variable argument list which is forwarded to storage policy.
         * Ideally if a policy requires compile time constants, it should be wrapped up in std::integral_constant.
         * @sa #INT(i)
         * @sa QueueNode
         * @sa #CircularQMultiAccess(T, S, ...)
         *
         */
        template <typename T, template <typename...> class Storage, typename... Args>
        class CircularQueue : private Storage<QueueNode<T>, Args...>
        {
        private:
            static const int MAX_SIZE;
            /* void store(int index, QueueNode<T>* value); */
            QueueNode<T> *load(int index);

            std::atomic_long tail;
            std::atomic_int writerCount;
            std::atomic_int readerCount;
            void addOne(int64_t &index);
            int64_t getMaxSize()
            {
                return Storage<QueueNode<T>, Args...>::m_size;
            }

            friend class _CircularQReaderType_(T, Storage, isMultiAccess);
            friend class _CircularQSWType_(T, Storage, isMultiAccess);

        public:
            CircularQueue();

            std::atomic_long head;

            /*!
             * Can be used to create single or multiple instances of the reader.
             * Only supports read operations.
             */
            _CircularQReaderType_(T, Storage, isMultiAccess) * createReader()
            {
                readerCount++;
                return new _CircularQReaderType_(T, Storage, isMultiAccess)(this);
            }

            /*!
             * Creates a writer for the queue. Only a single instance can be created for this queue.
             * Throws an exception if tried to create multiple instances.
             * The writer only supports write operations.
             */
            _CircularQSWType_(T, Storage, isMultiAccess) * createSingleWriter()
            {
                writerCount++; // This op is an atomic op (fetch-add)

                if (writerCount == 1)
                {
                    return new _CircularQSWType_(T, Storage, isMultiAccess)(this);
                }

                writerCount--; // To maintain the writerCount to 1 if an instance is present.
                throw "Only single instance allowed";
            }

            /*!
             * This method is not yet implemented.
             */
            void *createMultiWriter()
            {
                throw "Not yet implemented";
            }
        };

        /*!
         * \brief Circular queue without concurrency support.
         *
         * No synchronization is provided and hence is not thread safe.
         * Allows to specify storage policy as a template parameter. Following are the requirements of the storage policy
         * 1. Can have any number of template parameters.
         * 2. First parameter should always be the type of the data to be stored.
         * 3. Compile time constants should be wrapped using std::integral_constant.
         * 4. Should provide method @c at with the signature
         * @code
         * T* at(int64_t pos)
         * @endcode
         * 5. Should provide member @c m_size of type int64_t
         *
         * @tparam T Type of data to be stored
         * @tparam Storage Storage policy
         * @tparam Args Variable argument list which is forwarded to storage policy.
         * Ideally if a policy requires compile time constants, it should be wrapped up in std::integral_constant.
         * @sa #INT(i)
         */

        template <typename T, template <typename...> class Storage, typename... Args>
        class SimpleCircularQueue : private Storage<QueueNode<T>, Args...>
        {
        private:
            std::atomic_long head;
            std::atomic_long tail;
            QueueNode<T> *load(int index)
            {
                return Storage<QueueNode<T>, Args...>::at(index);
            }
            /* void store ( int index, QueueNode< T >* value ) */
            /* { */
            /*    *(Storage<QueueNode<T>, Args...>::at(index)) = *value; */
            /* } */
        public:
            SimpleCircularQueue()
            {
                head = -1;
                tail = -1;
            }

            void setSize(size_t size)
            {
                Storage<QueueNode<T>, Args...>::setStorage(size);
            }

            int64_t addOne(int64_t index)
            {
                return ((index + 1) % Storage<QueueNode<T>, Args...>::m_size);
            }

            void reset()
            {
                head = -1;
                tail = -1;
            }

            bool isEmpty()
            {
                return tail == head;
            }

            bool isFull()
            {
                if (tail == -1 && head == -1)
                    return false;
                else if (tail == -1 && addOne(head) == 0)
                    return true;
                else
                    return (addOne(head) == tail);
            }

            /*!
             * fronts
             */
            T front()
            {
                return load(addOne(tail))->value;
            }
            T *frontPtr()
            {
                QueueNode<T> *value = load(addOne(tail));
                return (&value->value);
            }
            /*!
             * Gets the front item from the queue and moves the tail pointer ahead.
             */
            T dequeue()
            {
                tail = addOne(tail);
                QueueNode<T> *value = load(tail);
                return value->value;
            }
            /*!
             * Gets the pointer to the front item from the queue and moves the tail pointer ahead.
             */
            T *dequeuePtr()
            {
                tail = addOne(tail);
                QueueNode<T> *value = load(tail);
                return (&value->value);
            }
            /*!
             * Does an empty check before dequeue. If queue is empty, returns false.
             * @param value Pointer to variable where the removed value will be stored.
             * Pointer is not changed if queue is empty.
             */
            bool dequeueWithEmptyCheck(T *value)
            {
                if (isEmpty())
                    return false;

                *value = dequeue();
                return true;
            }

            /*!
             * Adds value to the front of the queue.
             * No bounds check is performed. Should call @c isFull() if flow control is required.
             */
            void enqueue(T &value)
            {
                head = addOne(head);
                load(head)->value = value;
                load(head)->writeCount++;
            }

            /*!
             * Enqueues with overflow check. If queue is full, returns false.
             */
            bool enqueueWithOverflowCheck(T &value)
            {
                if (isFull())
                    return false;

                enqueue(value);
                return true;
            }

            /*!
             * getter to get the value at HEAD
             */
            T getFirstNodeValue()
            {
                QueueNode<T> *value = load(addOne(tail));
                return value->value;
            }
            /*!
             * getter to get the value at TAIL
             */
            T getLastNodeValue()
            {
                int32_t maxIndex = Storage<QueueNode<T>, Args...>::m_size;
                int32_t lastIndex = (maxIndex + head) % Storage<QueueNode<T>, Args...>::m_size;
                QueueNode<T> *value = load(lastIndex);
                return value->value;
            }
        };

    }
}

#endif // QUEUEBASE_H

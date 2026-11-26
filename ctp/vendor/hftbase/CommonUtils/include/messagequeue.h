#ifndef ILLUMINATI_MESSAGE_QUEUE
#define ILLUMINATI_MESSAGE_QUEUE

#include "c11compatible.h"
#include <sched.h>
#include <iostream>

namespace illuminati
{
    template <typename T>
    struct Message
    {
        T data;
        uint64_t seqNo;
    };

    /*!
     * \brief Circular multi-writer multi-reader queue.
     * @tparam T Type of data to be stored
     */
    template <typename T>
    class MessageQueue
    {
    private:
        std::atomic<int64_t> m_head;
        std::atomic<int64_t> tail __attribute__((aligned(64)));
        Message<T> *queue;
        size_t m_size;

        int64_t getMinHighestPowOf2(int64_t value)
        {
            if (!value & (!(value & (value - 1))))
            {
                return value;
            }
            int64_t result = 1;
            while (value)
            {
                value = value >> 1;
                result = result << 1;
            }
            return result;
        }

    public:
        MessageQueue()
        {
            init(1000000);
        }

        MessageQueue(size_t size)
        {
            init(size);
        }

        void init(size_t size)
        {
            m_size = getMinHighestPowOf2(size);
            queue = (Message<T> *)calloc(m_size, sizeof(Message<T>));
            m_head = 1;
            tail = 1;

            // load the entire queue in residual memory
            Message<T> tmp;
            for (int i = 0; i < m_size; i++)
            {
                memcpy(&tmp, queue + i, sizeof(Message<T>));
                Message<T> *slot = queue + i;
                slot->seqNo = 0;
            }
        }

        inline __attribute__((always_inline)) bool push(const T &value)
        {
            if (isFull())
                return false;
            int64_t myHead = m_head.fetch_add(1, std::memory_order_acq_rel);

            Message<T> *slot = queue + (myHead & (m_size - 1));
            slot->data = std::move(value);
            asm volatile("" ::
                             : "memory");
            slot->seqNo = myHead;

            return true;
        }

        inline __attribute__((always_inline)) void pop_block(T &data)
        {
            while (isEmpty())
            {
                sched_yield();
            }
            // std::cout << tail.load(std::memory_order_relaxed) << " " << m_size << " " << (tail.load(std::memory_order_relaxed) & (m_size-1)) << "\n";
            Message<T> *value = queue + (tail.load(std::memory_order_relaxed) & (m_size - 1));
            data = value->data;
            value->data = 0;
            tail = value->seqNo + 1;
            return;
        }

        inline __attribute__((always_inline)) bool isEmpty() __attribute__((hot))
        {
            uint64_t mytail = tail.load(std::memory_order_relaxed);
            return (queue + (mytail & (m_size - 1)))->seqNo < mytail;
        }

        inline __attribute__((always_inline)) bool isFull()
        {
            // std::cout << m_head.load(std::memory_order_relaxed) << " " << tail.load(std::memory_order_relaxed) << "\n";
            return ((m_head.load(std::memory_order_relaxed) - tail.load(std::memory_order_relaxed)) >= (m_size - 100));
        }
    };
}

#endif /* MESSAGE_QUEUE */

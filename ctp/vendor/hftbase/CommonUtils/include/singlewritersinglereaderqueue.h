#ifndef ILLUMINATI_SWSR_Q
#define ILLUMINATI_SWSR_Q

#include "c11compatible.h"
#include <sched.h>
// #include <iostream>

namespace illuminati
{
    /*!
     * \brief Circular multi-writer multi-reader queue.
     * @tparam T Type of data to be stored
     */
    /** WARNING: size must be a power of 2 minus 1*/
    template <typename T, uint64_t size>
    class SingleWriterSingleReaderQueue
    {
    private:
        struct Message
        {
            T data;
            uint64_t dirty;
        };

        Message *queue;
        std::atomic<int64_t> m_head __attribute__((aligned(64)));
        std::atomic<int64_t> m_tail __attribute__((aligned(64)));

    public:
        SingleWriterSingleReaderQueue()
        {
            init();
        }

        void init()
        {
            queue = (Message *)calloc(size, sizeof(Message));
            m_head = 1;
            m_tail = 1;
        }

        inline __attribute__((always_inline)) void enqueue(const T &value) __attribute__((hot))
        {
            // int64_t myHead = m_head.fetch_add(1, std::memory_order_acq_rel);
            int64_t myHead = m_head.load(std::memory_order_relaxed);
            m_head.store(myHead + 1, std::memory_order_relaxed);
            Message *slot = queue + (myHead & (size - 1));
            memcpy((void *)(&(slot->data)), (void *)(&value), sizeof(T));
            asm volatile("" ::
                             : "memory");
            slot->dirty = 1;
        }

        inline __attribute__((always_inline))
        T *
        getDequeuePtr() __attribute__((hot))
        {
            Message *value = queue + (m_tail.load(std::memory_order_relaxed) & (size - 1));
            return &(value->data);
        }

        inline __attribute__((always_inline))
        T *
        getDequeuePtr_unchecked() __attribute__((hot))
        {
            Message *value = queue + (m_tail.load(std::memory_order_relaxed) & (size - 1));
            return &(value->data);
        }

        inline __attribute__((always_inline)) void commitDequeue() __attribute__((hot))
        {
            int64_t myTail = m_tail.load(std::memory_order_relaxed);
            m_tail.store(myTail + 1, std::memory_order_relaxed);
            (queue + (myTail & (size - 1)))->dirty = 0;
        }

        inline __attribute__((always_inline))
        T
        dequeue() __attribute__((hot))
        {
            int64_t myTail = m_tail.load(std::memory_order_relaxed);
            Message *value = queue + (myTail & (size - 1));
            T tmp = value->data;
            asm volatile("" ::
                             : "memory");
            m_tail.store(myTail + 1, std::memory_order_relaxed);
            (queue + (myTail & (size - 1)))->dirty = 0;
            return tmp;
        }

        inline __attribute__((always_inline)) void prefetch()
        {
        }

        inline __attribute__((always_inline)) bool isEmpty() __attribute__((hot))
        {
            uint64_t mytail = m_tail.load(std::memory_order_relaxed);
            return ((queue + (mytail & (size - 1)))->dirty == 0);
        }

        inline __attribute__((always_inline)) bool isFull() __attribute__((hot))
        {
            // std::cout << m_head.load(std::memory_order_relaxed) << " " << tail.load(std::memory_order_relaxed) << "\n";
            return ((queue + (m_head.load(std::memory_order_relaxed) & (size - 1)))->dirty != 0);
        }

        void setSize(uint64_t a)
        {
            // to make it easier to switch between this and locklessringbuffer in the sim code. Temporary
        }

        /**** for compatibility with CircularQMultiAccess */
        SingleWriterSingleReaderQueue<T, size> *createReader()
        {
            return this;
        }

        SingleWriterSingleReaderQueue<T, size> *createSingleWriter()
        {
            return this;
        }

        // DO NOT USE
        inline __attribute__((always_inline))
        uint64_t
        getRemainingSize() __attribute__((hot))
        {
            return 1;
            //  return size - (m_head.load(std::memory_order_relaxed) - m_tail.load(std::memory_order_relaxed));
        }
    };
}

#endif /* MESSAGE_QUEUE */

#ifndef LOCKLESSRINGBUFFER_H_
#define LOCKLESSRINGBUFFER_H_

#include <cstdint>
#include <cstring>
#include "c11compatible.h"

#define FORCE_INLINE inline __attribute__((always_inline))

namespace illuminati
{
    namespace ds
    {
        template <typename Node>
        struct LocklessRingBuffer
        {
        private:
            Node *queue_;
            int64_t queue_size_;

            std::atomic<int64_t> read_index_;
            std::atomic<int64_t> write_index_;

            int64_t next_record_;
            int64_t current_record_;

        public:
            LocklessRingBuffer() : queue_(nullptr), queue_size_(0), read_index_(0), write_index_(0), next_record_(0), current_record_(0)
            {
            }

            ~LocklessRingBuffer()
            {
                delete[] queue_;
            }

            FORCE_INLINE void setSize(int64_t queue_size)
            {
                queue_size_ = queue_size;
                queue_ = new Node[queue_size_];
            }

            FORCE_INLINE void enqueue(Node &node)
            {
                enqueue(&node);
            }

            FORCE_INLINE void enqueue(Node *nodeptr)
            {
                auto const current_write = write_index_.load(std::memory_order_relaxed);

                next_record_ = current_write + 1;
                if (next_record_ == queue_size_)
                    next_record_ = 0;

                memcpy((char *)&queue_[next_record_], (char *)nodeptr, sizeof(Node));
                write_index_.store(next_record_, std::memory_order_release);
            }

            FORCE_INLINE Node *getEnqueuePtr()
            {
                auto const current_write = write_index_.load(std::memory_order_relaxed);

                next_record_ = current_write + 1;
                if (next_record_ == queue_size_)
                    next_record_ = 0;

                return &queue_[next_record_];
            }

            FORCE_INLINE void commitEnqueue()
            {
                write_index_.store(next_record_, std::memory_order_release);
            }

            FORCE_INLINE Node *dequeue()
            {
                auto const current_read = read_index_.load(std::memory_order_acquire);
                auto const current_write = write_index_.load(std::memory_order_acquire);

                if (current_read == current_write)
                    return nullptr;

                current_record_ = current_read + 1;
                if (current_record_ == queue_size_)
                    current_record_ = 0;

                read_index_.store(current_record_, std::memory_order_release);

                return &queue_[current_record_];
            }

            FORCE_INLINE Node *getDequeuePtr()
            {
                auto const current_read = read_index_.load(std::memory_order_acquire);
                auto const current_write = write_index_.load(std::memory_order_acquire);

                if (current_read == current_write)
                    return nullptr;

                current_record_ = current_read + 1;
                if (current_record_ == queue_size_)
                    current_record_ = 0;

                return &queue_[current_record_];
            }

            FORCE_INLINE void commitDequeue()
            {
                read_index_.store(current_record_, std::memory_order_release);
            }

            FORCE_INLINE bool isEmpty()
            {
                auto const current_read = read_index_.load(std::memory_order_acquire);
                auto const current_write = write_index_.load(std::memory_order_acquire);

                return (current_read == current_write);
            }

            FORCE_INLINE bool isFull()
            {
                auto const current_write = write_index_.load(std::memory_order_relaxed);

                if (((current_write + 1) % queue_size_) == read_index_.load())
                    return true;

                return false;
            }

            FORCE_INLINE Node *peekFirstElement()
            {
                return getDequeuePtr();
            }

            // Clear is only to be used when no thread is currently using the ringbuffer, Warning this could lead to some seriously undefined behaviour.
            FORCE_INLINE void clear()
            {
                read_index_.store(0);
                write_index_.store(0);
            }
        };
    }
}
#endif /* LOCKLESSRINGBUFFER_H_ */

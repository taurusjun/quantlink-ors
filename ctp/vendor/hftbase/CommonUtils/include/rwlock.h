#ifndef _RWLOCK_H_
#define _RWLOCK_H_
#include "c11compatible.h"
#include <assert.h>
namespace illuminati
{
    namespace ds
    {
        class RWLock
        {
        protected:
            std::atomic<int32_t> lock_;

            static const unsigned int WRITER_FLAG = 1;
            static const unsigned int READER_INC = 2;

        public:
            RWLock()
            {
                lock_.store(0);
            }
            void rlock()
            {
                lock_.fetch_add(READER_INC);
                asm volatile("" ::: "memory");
                while (((lock_.load(std::memory_order_relaxed)) & WRITER_FLAG) == 1)
                    asm volatile("pause" ::: "memory");
            }

            void runlock()
            {
                auto tmp = lock_.fetch_add(-READER_INC);
                assert(lock_.load(std::memory_order_relaxed) >= 0);
            }

            void wlock()
            {
                int32_t expected = 0;
                while (!lock_.compare_exchange_weak(expected, 1))
                {
                    expected = 0;
                    asm volatile("pause" ::: "memory");
                }
            }

            void wunlock()
            {
                lock_.fetch_add(-WRITER_FLAG);
                assert(lock_.load(std::memory_order_relaxed) >= 0);
            }
        };
    } // namespace ds
} // namespace illuminati
#endif

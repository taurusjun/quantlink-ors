#ifndef TICKETLOCK_H
#define TICKETLOCK_H

#include "c11compatible.h"
#include <limits>
// #include "log.h"
// using namespace illuminati::logger;
namespace illuminati
{
    namespace ds
    {
        class TicketLock
        {
        private:
            std::atomic<uint64_t> lock_;
            const uint64_t mask = 0x00000000ffffffffull;
            const uint64_t incr = 0x0000000100000000ull;

        public:
            TicketLock() : lock_(0)
            {
            }

            inline bool isLocked()
            {
                uint64_t tmp = lock_.load(std::memory_order_relaxed);

                uint32_t queue = ((tmp >> 32) & mask);
                uint32_t ticket = tmp & mask;

                return ticket != queue + 1;
            }

            inline bool isContended()
            {
                uint64_t tmp = lock_.load(std::memory_order_relaxed);

                uint32_t queue = ((tmp >> 32) & mask);
                uint32_t ticket = tmp & mask;

                if (queue >= ticket)
                    return queue - ticket > 1;
                else
                    return (std::numeric_limits<uint64_t>::max() - (ticket - queue)) > 0;
            }

            inline int waitingThreads()
            {
                uint64_t tmp = lock_.load(std::memory_order_relaxed);

                uint32_t queue = tmp >> 32;
                uint32_t ticket = tmp & mask;
                // ILOG(INFO) << "Lock : " << tmp << " queue: " << queue << " ticket: " << ticket  <<"\n";

                if (queue > ticket)
                    return queue - ticket - 1;
                else if (queue == ticket)
                    return 0;
                else
                    // TODO : test
                    return (std::numeric_limits<uint64_t>::max() - (ticket - queue));
            }

            inline void lock()
            {
                uint64_t myticket = lock_.fetch_add(incr, std::memory_order_relaxed) >> 32;

                /*
                uint64_t tmp = lock_.load(std::memory_order_relaxed);

                uint32_t queue = tmp >> 32;
                uint32_t ticket = tmp & mask;
                ILOG(INFO) << "Locking, Lock : " << tmp << " queue: " << queue << " ticket: " << ticket  <<"\n";

                ILOG(INFO) << "myticket : " << myticket << "\n";
                */

                while (myticket != (lock_.load(std::memory_order_relaxed) & mask))
                {
                    asm volatile("pause" ::: "memory");
                }
            }

            inline void unlock()
            {
                // increment lower 32 bits without overflowing into the higher bits
                ((std::atomic<uint32_t> *)&lock_)->fetch_add(1, std::memory_order_relaxed);

                /*
                uint64_t tmp = lock_.load(std::memory_order_relaxed);
                uint32_t queue = tmp >> 32;
                uint32_t ticket = tmp & mask;
                ILOG(INFO) << "Unlockig, Lock : " << tmp << " queue: " << queue << " ticket: " << ticket  <<"\n";
                */
            }

            inline bool tryLock()
            {
                uint64_t tmp = lock_.load(std::memory_order_relaxed);
                uint32_t queue = tmp >> 32;
                uint32_t ticket = tmp & mask;

                // ILOG(INFO) << "Lock : " << tmp << " queue: " << queue << " ticket: " << ticket  <<"\n";

                if (ticket == queue)
                    return lock_.compare_exchange_strong(tmp, tmp + incr,
                                                         std::memory_order_relaxed, std::memory_order_relaxed);
                else
                    return false;
            }
        };
    }
}

#endif

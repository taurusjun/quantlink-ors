#ifndef _INFRA_TESTANDSETLOCK_H
#define _INFRA_TESTANDSETLOCK_H

#include "c11compatible.h"
namespace illuminati
{
    namespace ds
    {
        class TestAndSetLock
        {
            std::atomic_flag _lock;

        public:
            TestAndSetLock() : _lock(ATOMIC_FLAG_INIT) {}

            // TODO: Improve this by adding Test and TestAndSet,
            //  Arun:: I don't see a boolean test operation is possible on the atomic_flag
            void lock(std::memory_order mo = std::memory_order_acquire)
            {
                while (_lock.test_and_set(mo))
                {
                    asm volatile("pause" ::: "memory");
                }
            }
            void unlock(std::memory_order mo = std::memory_order_acquire)
            {
                _lock.clear(mo);
            }
        };
    }
}

#endif
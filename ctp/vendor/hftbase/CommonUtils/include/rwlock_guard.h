#ifndef RWLOCKGUARD_H_
#define RWLOCKGUARD_H_
#include <mutex>
namespace illuminati
{
    namespace ds
    {
        /// @brief  Scoped reader lock idiom.
        // Acquire a reader lock here with a constructor call, then release with
        // the destructor call in accordance with RAII style.
        template <typename _RLock>
        class rlock_guard
        {
        public:
            typedef _RLock rlock_type;

            explicit rlock_guard(rlock_type &__r) : _M_device(__r)
            {
                _M_device.rlock();
            }

            rlock_guard(rlock_type &__r, std::adopt_lock_t) : _M_device(__r)
            {
            } // calling thread owns mutex

            ~rlock_guard()
            {
                _M_device.runlock();
            }

            rlock_guard(const rlock_guard &) = delete;
            rlock_guard &operator=(const rlock_guard &) = delete;

        private:
            rlock_type &_M_device;
        };

        /// @brief  Scoped writer lock idiom.
        // Acquire a writer lock here with a constructor call, then release with
        // the destructor call in accordance with RAII style.
        template <typename _WLock>
        class wlock_guard
        {
        public:
            typedef _WLock wlock_type;

            explicit wlock_guard(wlock_type &__w) : _M_device(__w)
            {
                _M_device.wlock();
            }

            wlock_guard(wlock_type &__w, std::adopt_lock_t) : _M_device(__w)
            {
            } // calling thread owns mutex

            ~wlock_guard()
            {
                _M_device.wunlock();
            }

            wlock_guard(const wlock_guard &) = delete;
            wlock_guard &operator=(const wlock_guard &) = delete;

        private:
            wlock_type &_M_device;
        };

        template <typename _RLock, bool enable>
        class maybe_rlock_guard;

        template <typename _RLock>
        class maybe_rlock_guard<_RLock, true>
        {
        public:
            typedef _RLock rlock_type;

            explicit maybe_rlock_guard(rlock_type &__r) : _M_device(__r)
            {
                _M_device.rlock();
            }

            maybe_rlock_guard(rlock_type &__r, std::adopt_lock_t) : _M_device(__r)
            {
            } // calling thread owns mutex

            ~maybe_rlock_guard()
            {
                _M_device.runlock();
            }

            maybe_rlock_guard(const maybe_rlock_guard &) = delete;
            maybe_rlock_guard &operator=(const maybe_rlock_guard &) = delete;

        private:
            rlock_type &_M_device;
        };

        template <typename _RLock>
        class maybe_rlock_guard<_RLock, false>
        {
        public:
            typedef _RLock rlock_type;
            explicit maybe_rlock_guard(rlock_type &__r) {}
            maybe_rlock_guard(rlock_type &__r, std::adopt_lock_t) {}
        };
    }
} // namespace illuminati
#endif

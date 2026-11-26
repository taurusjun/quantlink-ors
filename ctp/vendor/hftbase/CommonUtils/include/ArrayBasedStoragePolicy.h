#ifndef ILLUMINATI_SP_ARRAYBASEDSTORAGEPOLICY_H
#define ILLUMINATI_SP_ARRAYBASEDSTORAGEPOLICY_H

// #include "commonmacros.h"
#include <string.h>

namespace illuminati
{
    namespace storagepolicy
    {
        template <class T, class U>
        class ArrayStorage
        {
        public:
            ArrayStorage()
            {
                m_size = U::value;
                m_updates = m_storage;
            }
            int64_t getSize()
            {
                return m_size;
            }

        protected:
            T *at(int64_t pos)
            {
                T *result = m_updates + pos;
                return result;
            }
            /*!
             * dummy function to conform with other policy classes
             */
            void setStorage(uint64_t size)
            {
            }

            bool store(int64_t pos, T value)
            {
                // we are using memcpy to utilize the performance gain. memcpy only does a shallow copy and can cause issues if T has an overloaded = operator.
                /* *(m_updates+pos) = *value; */
                memcpy(m_updates + pos, &value, sizeof(T));
                return true;
            }

            T m_storage[U::value ? U::value : 1] __attribute__((aligned(64)));
            int64_t m_size;
            T *m_updates;
        };
    }
}

#endif

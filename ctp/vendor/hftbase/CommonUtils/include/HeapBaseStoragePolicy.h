#ifndef ILLUMINATI_SP_HEAPBASEDSTORAGEPOLICY_H
#define ILLUMINATI_SP_HEAPBASEDSTORAGEPOLICY_H

#include <string.h>

namespace illuminati
{
    namespace storagepolicy
    {
        template <class T>
        class HeapStorage
        {
        public:
            HeapStorage()
            {
                m_size = 0;
                m_storage = NULL;
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

            void setStorage(uint64_t size)
            {
                m_storage = new T[size];
                m_updates = m_storage;
                m_size = size;
            }

            bool store(int64_t pos, T value)
            {
                // we are using memcpy to utilize the performance gain. memcpy only does a shallow copy and can cause issues if T has an overloaded = operator.
                memcpy(m_updates + pos, &value, sizeof(T));
                return true;
            }

            T *m_storage;
            int64_t m_size;
            T *m_updates;
        };
    }
}

#endif

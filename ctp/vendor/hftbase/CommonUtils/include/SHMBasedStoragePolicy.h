#ifndef ILLUMINATI_SP_SHMBASEDSTORAGE_H
#define ILLUMINATI_SP_SHMBASEDSTORAGE_H

#include <inttypes.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <iostream>
#include <cassert>
#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include "c11compatible.h"

namespace illuminati
{
    namespace storagepolicy
    {
        struct MessageHeader
        {
            std::atomic_long head;
            MessageHeader()
            {
                /* Initial value of the Queue local head is assumed to be 0. Queue is the one who advances this head.
                   So the initial value of shm head should be -1.  */
                head.store(-1);
            }
        };

        /* struct Notifications */
        /* { */
        /*      Notifications () */
        /*      { */
        /*          messageCount = 0 ; */
        /*          isActive = false ; */
        /*      } */
        /*      void NotifyClients () */
        /*      { */
        /*          messageCount++ ; */
        /*      } */
        /*      uint64_t RegisterClient () */
        /*      { */
        /*          return messageCount ; */
        /*      } */
        /*      void WaitForUpdate ( uint64_t clientMessageCount ) */
        /*      { */
        /*          while( clientMessageCount == messageCount ) ; */
        /*      } */
        /*      volatile uint64_t   messageCount ; */
        /*      volatile bool       isActive ; */
        /* }; */

        /**
         * @tparam T Type to be written to shared memory
         * @tparam U Constant whose value is the maximum size of shared memory
         * @tparam V Constant which specifies the shared memory key. Any other process which needs to access this shared memory should use the same key.
         * All the values of key should be present in constants.h
         */
        template <class T, class U, class V, class W = std::integral_constant<int, IPC_CREAT | 0666>>
        class SHMStorage
        {
        public:
            SHMStorage()
            {
                size_t m_shmsize = U::value;
                m_size = (m_shmsize - sizeof(MessageHeader)) / sizeof(T);
                int32_t m_shmid = shmget(V::value, m_shmsize, W::value);

                if (m_shmid < 0)
                {
                    std::cerr << "shmget error: " << std::strerror(errno) << std::endl;
                    assert(0);
                }

                std::cout << "SHMID = " << m_shmid << "\n";

                if ((m_shmadr = shmat(m_shmid, NULL, 0)) == (void *)-1)
                {
                    std::cerr << "Shm attach error: " << std::strerror(errno) << std::endl;
                    assert(0);
                }

                if ((W::value & IPC_CREAT) == IPC_CREAT)
                    header = new (m_shmadr) MessageHeader();
                else
                    header = (MessageHeader *)m_shmadr;

                /* m_notifier = new(m_shmadr) Notifications() ; */
                m_updates = (T *)(((char *)m_shmadr) + sizeof(MessageHeader));

                std::cout << "Header count: " << header->head.load() << "\n";
            }

        protected:
            T *at(int64_t pos)
            {
                T *result = m_updates + pos;
                return result;
            }

            /*!
             * unimplemented function to conform with other policy classes
             * it can be used to set the shm size
             */
            void setStorage(uint64_t size)
            {
            }

            bool store(int64_t pos, T value)
            {
                // we are using memcpy to utilize the performance gain. memcpy only does a shallow copy and can cause issues if T has an overloaded = operator.
                /* *(m_updates+pos) = *value; */
                memcpy(m_updates + pos, &value, sizeof(T));
                header->head.store(pos);
                /* std::cout<<"Head = "<<pos<<"\n"; */
                return true;
            }

            void *m_shmadr;
            /* Notifications* m_notifier; */
            MessageHeader *header;
            T *m_updates;
            size_t m_size;
        };
    }
}

#endif

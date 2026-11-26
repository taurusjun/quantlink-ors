/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef UNORDERED_LOCKLESS_MAP_H
#define UNORDERED_LOCKLESS_MAP_H

#include "c11compatible.h"
#include <stdlib.h>
#include <functional>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <caslock.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <map>
#include "caslock.h"
#include <cstring>

#ifdef ENABLE_PERF_MONITOR
#include "logger.h"
#endif

namespace illuminati
{
    namespace ds
    {

        template <typename ValueType>
        class MapMember
        {
        public:
            // used for ordering read after write.
            std::atomic_bool isEmpty;
            int64_t key;
            ValueType value;
            std::atomic<long> keyCheck;
            typedef std::unordered_map<uint64_t, ValueType> BucketType;
            BucketType *bucket;
            CasLock lock;
#ifdef ENABLE_PERF_MONITOR
            uint64_t bucketAccessCount;
            uint64_t maxBucketSize;
            uint64_t totalAccessCount;
#endif

            MapMember() : bucket(nullptr)
            {
                isEmpty.store(true);
                keyCheck = -1;
#ifdef ENABLE_PERF_MONITOR
                bucketAccessCount = 0;
                maxBucketSize = 0;
                totalAccessCount = 0;
                key = 0;
                // memset(&value, 0, sizeof(ValueType));
#endif
            }
            void setValue(const int64_t &key, const ValueType &value)
            {
#ifdef ENABLE_PERF_MONITOR
//                 ILOG(INFO) << "setValue: " << key << ":" << value.GetSequence() << "\n";
#endif
                long temp = -1;
                // if(!isEmpty.exchange(false, std::memory_order_acq_rel))
                if (!keyCheck.compare_exchange_strong(temp, (long)key))
                {
                    if (temp == (long)key)
                    {
                        return;
                    }
                    // already occupied, so add to the bucket.
                    lock.lock(std::memory_order_acq_rel);
                    if (bucket == nullptr)
                    {
                        bucket = new BucketType;
#ifdef ENABLE_PERF_MONITOR
//                         ILOG(FATAL) << "Creating new bucket for " << key << "\n";
#endif
                    }
#ifdef ENABLE_PERF_MONITOR
//                     ILOG(FATAL) << "inserting into bucket key = " << key << "\n";
#endif
                    bucket->insert(std::make_pair(key, value));

#ifdef ENABLE_PERF_MONITOR
                    bucketAccessCount++;
                    if (maxBucketSize < bucket->size())
                        maxBucketSize = bucket->size();
                    if (bucketAccessCount % 1000 == 0)
                    {
                        ILOG(INFO) << "Total access count=" << totalAccessCount;
                        ILOG(INFO) << " : Bucket access count=" << bucketAccessCount;
                        ILOG(INFO) << " : Max bucket size=" << maxBucketSize << "\n";
                    }
#endif

                    lock.unlock(std::memory_order_release);
                }
                else
                {
                    this->value = value;
                    asm volatile("" : : : "memory");
                    this->key = key;
                    isEmpty.store(false, std::memory_order_release);
                }
#ifdef ENABLE_PERF_MONITOR
                totalAccessCount++;
#endif
            }
        };

        template <typename Factor>
        class DefaultHashFunc
        {
        public:
            size_t operator()(const uint64_t &key)
            {
                return key % Factor::value;
            }
        };

        template <typename Factor>
        class DefaultReHashFunc
        {
        public:
            size_t operator()(const uint64_t &key, int tryCount = 0)
            {
                int value = key / Factor::value;
                if (value >= Factor::value)
                    value = value % Factor::value;
                return value;
            }
        };

        template <typename ValueType, typename Maxsize,
                  typename Hash = DefaultHashFunc<Maxsize>, typename ReHash = DefaultReHashFunc<Maxsize>, typename Predicate = std::equal_to<uint64_t>>
        class unordered_lockless_map
        {
        private:
            CasLock lock;
            std::atomic<uint64_t> firstKey;
            std::atomic<bool> bfirstKeySet;

        public:
            // typedef unordered_lockless_map< KeyType, ValueType, Maxsize, Hash, ReHash, Predicate > BucketMap;
            MapMember<ValueType> container[Maxsize::value];
            Hash hashFunc;
            ReHash rehashFunc;
            Predicate predicate;
            // CasLock spinLock;

            // std::mutex lock;

            int64_t lastContinuousKey;

        public:
            unordered_lockless_map()
            {
                bfirstKeySet = false;
                lastContinuousKey = -1;
            }

            unordered_lockless_map(const unordered_lockless_map &) = delete;
            unordered_lockless_map &operator=(const unordered_lockless_map &) = delete;

            void setFirstKey(const uint64_t &key)
            {
                lock.lock();
                this->firstKey = key;
                bfirstKeySet = true;
                lock.unlock();
            }

            bool getFirstKey(uint64_t &key)
            {
                bool retVal = false;
                lock.lock();
                key = firstKey;
                retVal = bfirstKeySet;
                lock.unlock();

                return retVal;
            }

            void insert(const int64_t &key, const ValueType &value);
            /**
             * Retrieves the mapped value for the specified key.
             * @param[in] key Key for which value is retrieved
             * @param[out] value Retrieved value
             * @return Returns true if a value is mapped for key.
             */
            bool at(const int64_t &key, ValueType &value, bool markAsEmpty = true);
            /**
             * Returns a reference to the mapped value for the specified key.
             * If no value is found, inserts a default value and returns its reference.
             */
            ValueType &operator[](const uint64_t &key);

            bool find(const uint64_t &key);

            /*
            void print(std::string parentIndex="0", int level=0)
            {
                for(int index=0; index<Maxsize::value; index++)
                {
                    if(!(container[index].isEmpty))
                    {
                        std::cout<<"ParentIndex: "<<parentIndex<<" Index: "<<index<<" Level: "<<level<<" key: "<<(container[index].key)<<" value: "<<(container[index].value)<<"\n";
                    }
                    if(container[index].bucket != nullptr)
                    {
                        std::string tempIndex = parentIndex + ":";
                        tempIndex += std::to_string((long long)index);
                        container[index].bucket->print(tempIndex, level + 1);
                    }
                }
            }
            */
            void print()
            {
                for (int index = 0; index < Maxsize::value; index++)
                {
                    if (container[index].keyCheck.load() != -1)
                    {
#ifdef ENABLE_PERF_MONITOR
                        ILOG(INFO) << "Index: " << index << " key: " << (container[index].key) << " value: " << (container[index].value) << "\n";
#endif
                    }
                    if (container[index].bucket != nullptr && container[index].bucket->size() > 0)
                    {
#ifdef ENABLE_PERF_MONITOR
                        ILOG(INFO) << "Bucket contents for index: " << index << "\n";
#endif
                        for (auto i = container[index].bucket->begin(); i != container[index].bucket->end(); i++)
                        {
#ifdef ENABLE_PERF_MONITOR
                            ILOG(INFO) << "Index: " << index << " key: " << i->first << " value: " << i->second << "\n";
#endif
                        }
                    }
                    if (container[index].keyCheck.load() != -1 &&
                        (container[index].bucket == nullptr || container[index].bucket->size() == 0))
                    {
#ifdef ENABLE_PERF_MONITOR
                        ILOG(INFO) << "Index: " << index << " is empty\n";
#endif
                    }
#ifdef ENABLE_PERF_MONITOR
                    ILOG(INFO) << "Total access count=" << container[index].totalAccessCount;
                    ILOG(INFO) << " : Bucket access count=" << container[index].bucketAccessCount;
                    ILOG(INFO) << " : Max bucket size=" << container[index].maxBucketSize << "\n";
#endif
                }
            }
        };

        template <typename KeyType, typename ValueType>
        class unordered_threadsafe_map
        {
        private:
            std::unordered_map<KeyType, ValueType> _container;
            std::mutex lock;
            KeyType firstKey;
            bool bfirstKeySet;

        public:
            unordered_threadsafe_map()
            {
                bfirstKeySet = false;
            }

            void setFirstKey(const KeyType &key)
            {
                lock.lock();
                this->firstKey = key;
                bfirstKeySet = true;
                lock.unlock();
            }

            bool getFirstKey(KeyType &key)
            {
                bool retVal = false;
                lock.lock();
                key = firstKey;
                retVal = bfirstKeySet;
                lock.unlock();

                return retVal;
            }

            void insert(const KeyType &key, const ValueType &value)
            {
                lock.lock();
                _container.insert(std::make_pair(key, value));
                lock.unlock();
            }
            bool at(const KeyType &key, ValueType &value, bool markAsEmpty = true)
            {
                lock.lock();
                auto index = _container.find(key);
                if (index == _container.end())
                {
                    lock.unlock();
                    return false;
                }
                value = index->second;
                if (markAsEmpty)
                {
                    _container.erase(index);
                }
                lock.unlock();
                return true;
            }
            ValueType operator[](const KeyType &key)
            {
                lock.lock();
                ValueType v = _container[key];
                lock.unlock();
                return v;
            }
            size_t size()
            {
                size_t value = 0;
                lock.lock();
                value = _container.size();
                lock.unlock();
                return value;
            }
            bool find(const KeyType &key)
            {
                lock.lock();
                auto index = _container.find(key);
                bool result = (index != _container.end());
                lock.unlock();
                return result;
            }
        };

        ///////////
        template <typename KeyType, typename ValueType>
        class threadsafe_multimap
        {
        private:
            std::multimap<KeyType, ValueType> _container;
            std::mutex lock;
            KeyType firstKey;
            bool bfirstKeySet;

        public:
            bool isEmpty()
            {
                bool retVal = false;
                lock.lock();
                retVal = _container.empty();
                lock.unlock();
                return retVal;
            }
            void erase(const KeyType &key)
            {
                lock.lock();
                _container.erase(key);
                lock.unlock();
            }
            void insert(const KeyType &key, const ValueType &value)
            {
                lock.lock();
                _container.insert(std::make_pair(key, value));
                lock.unlock();
            }
            bool at(const KeyType &key, std::vector<ValueType> &value, int &count)
            {
                count = 0;
                lock.lock();
                auto range = _container.equal_range(key);
                for (auto it = range.first; it != range.second; it++)
                {
                    value.push_back(it->second);
                    count++;
                }

                lock.unlock();
                return (count != 0);
            }
            bool find(const KeyType &key)
            {
                lock.lock();
                auto index = _container.find(key);
                bool result = (index != _container.end());
                lock.unlock();
                return result;
            }
        };
    }
}

#endif // UNORDERED_LOCKLESS_MAP_H

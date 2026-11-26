/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef UNORDERED_LOCKLESS_MAP_HPP
#define UNORDERED_LOCKLESS_MAP_HPP

#include "unordered_lockless_map.h"
#include <sstream>
#include <iostream>

#ifdef ENABLE_PERF_MONITOR
#include "logger.h"
#endif

using namespace illuminati::ds;

template <typename ValueType, typename Maxsize,
          typename Hash, typename ReHash, typename Predicate>
void unordered_lockless_map<ValueType, Maxsize, Hash, ReHash, Predicate>::insert(const int64_t &key, const ValueType &value)
{
    size_t index = hashFunc(key);
    if (key <= lastContinuousKey)
        return;
    //     ILOG(INFO)<<"Inserting "<<key<<"\n";
    container[index].setValue(key, value);
}

template <typename ValueType, typename Maxsize,
          typename Hash, typename ReHash, typename Predicate>
bool unordered_lockless_map<ValueType, Maxsize, Hash, ReHash, Predicate>::find(const uint64_t &key)
{
    size_t index = hashFunc(key);
    // Peek the value at index
    // single reader, so no contention.
    if (!container[index].isEmpty.load(std::memory_order_acquire) && container[index].key == key)
    {
        return true;
    }
    else
    {
        //         if(container[index].key <= key)
        //         {
        //             container[index].isEmpty.store(true, std::memory_order_acq_rel);
        //             container[index].keyCheck.store(-1, std::memory_order_acq_rel);
        //         }
        // check the bucket
        container[index].lock.lock(std::memory_order_acq_rel);
        auto bucket = container[index].bucket;
        if (bucket == nullptr)
        {
            container[index].lock.unlock(std::memory_order_release);
            return false;
        }
        auto idx = bucket->find(key);
        if (idx == bucket->end())
        {
            container[index].lock.unlock(std::memory_order_release);
            return false;
        }
        else
        {
            container[index].lock.unlock(std::memory_order_release);
            return true;
        }
    }
}

template <typename ValueType, typename Maxsize,
          typename Hash, typename ReHash, typename Predicate>
bool unordered_lockless_map<ValueType, Maxsize, Hash, ReHash, Predicate>::at(const int64_t &key, ValueType &value, bool markAsEmpty)
{
    size_t index = hashFunc(key);
    // Peek the value at index
    // single reader, so no contention.
    if (!container[index].isEmpty.load(std::memory_order_acquire) && container[index].key == key)
    // if(container[index].keyCheck.load(std::memory_order_acq_rel) != -1 && container[index].key == key)
    {
        // retrieve the value and reset the node.
        value = container[index].value;
        lastContinuousKey = key;
        container[index].isEmpty.store(true, std::memory_order_release);
        container[index].keyCheck.store(-1, std::memory_order_release);
    }
    else
    {
        if (!container[index].isEmpty.load(std::memory_order_acquire) && container[index].keyCheck.load(std::memory_order_acquire) < key)
        {
            // #ifdef ENABLE_PERF_MONITOR
            //             ILOG(INFO)<<"at(): setting "<<index<<" to empty: keycheck="<<container[index].keyCheck.load(std::memory_order_acq_rel)
            //                 <<" key="<<key<<" container key="<<container[index].key<<"\n";
            // #endif
            container[index].isEmpty.store(true, std::memory_order_release);
            container[index].keyCheck.store(-1, std::memory_order_release);
        }

        // check the bucket
        container[index].lock.lock(std::memory_order_acq_rel);
        auto bucket = container[index].bucket;
        if (bucket == nullptr)
        {
#ifdef ENABLE_PERF_MONITOR
//         ILOG(WARN) << "Bucket is empty\n";
#endif
            container[index].lock.unlock(std::memory_order_release);
            return false;
        }
        auto idx = bucket->find(key);
        if (idx == bucket->end())
        {
#ifdef ENABLE_PERF_MONITOR
//         ILOG(WARN) << "Bucket is not empty. but the key is not found\n";
#endif
            container[index].lock.unlock(std::memory_order_release);
            return false;
        }
        value = idx->second;
        bucket->erase(idx);
        container[index].lock.unlock(std::memory_order_release);
    }
    return true;
}

template <typename ValueType, typename Maxsize,
          typename Hash, typename ReHash, typename Predicate>
ValueType &unordered_lockless_map<ValueType, Maxsize, Hash, ReHash, Predicate>::operator[](const uint64_t &key)
{
    throw "unordered_lockless_map::operator[] not implemented";
}

#endif

#ifndef OBJECTPOOL_HPP_
#define OBJECTPOOL_HPP_
#include "objectpool.h"

using namespace illuminati::ds;

template <typename T>
void ObjectPool<T>::create_new_pool(unsigned int num, std::function<void(T *)> cb)
{
    if (num == 0)
        return;

    Chunk *pool = new Chunk[num];
    head = pool;
    pools.push(pool);
    numfree += num;

    Chunk *pool_elem = pool;
    for (int i = 0; i < num - 1; i++)
    {
        if (cb)
            cb(reinterpret_cast<T *>(pool_elem));

        pool_elem->next = pool_elem + 1;
        pool_elem++;
    }
    // for last element;
    pool_elem->next = head;
    if (cb)
        cb(reinterpret_cast<T *>(pool_elem));
}

template <typename T>
void ObjectPool<T>::init(unsigned int num)
{
    create_new_pool(num, std::function<void(T *)>());
}

template <typename T>
void ObjectPool<T>::init(unsigned int num, std::function<void(T *)> &cb)
{
    create_new_pool(num, cb);
}

template <typename T>
void ObjectPool<T>::extend(unsigned int num)
{
    create_new_pool(num, std::function<void(T *)>());
}

template <typename T>
void ObjectPool<T>::extend(unsigned int num, std::function<void(T *)> &cb)
{
    create_new_pool(num, cb);
}

template <typename T>
T *ObjectPool<T>::allocate()
{
    numfree--;
    Chunk *chunk = head;
    head = chunk->next;
    return reinterpret_cast<T *>(chunk);
}

template <typename T>
void ObjectPool<T>::deallocate(T *p)
{
    numfree++;
    Chunk *chunk = reinterpret_cast<Chunk *>(p);
    chunk->next = head;
    head = chunk;
}

template <typename T>
unsigned int ObjectPool<T>::available()
{
    return numfree;
}

template <typename T>
ObjectPool<T>::~ObjectPool<T>()
{
    while (pools.size() != 0)
    {
        delete[] pools.front();
        pools.pop();
    }
}
#endif

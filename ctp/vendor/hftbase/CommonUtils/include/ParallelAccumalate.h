#ifndef _PARALLELACCUMALATE_H
#define _PARALLELACCUMALATE_H

#include <algorithm>
#include <thread>
#include <functional>
#include <iostream>
#include <cmath>
#include "Thread.h"
#include <stdlib.h>

template <typename Iterator, typename T>
struct accumalate_block
{
    void operator()(Iterator first, Iterator last, T func_ /*std::function<void(T)> func_*/)
    {
        std::cout << "Inside the thread" << std::endl;
        while (first != last)
        {
            func_(*first);
            first++;
        }
    }

    void perform(Iterator first, Iterator last, T func_)
    {
        std::cout << "Inside the thread" << std::endl;
        while (first != last)
        {
            func_(*first);
            first++;
        }
    }
};

template <typename Iterator, typename T>
void parallel_operation(Iterator first, Iterator last, T func_ /*std::function<void(T)> func_*/)
{
    unsigned long const length = std::distance(first, last);

    char *value = getenv("MAX_THREADS");
    int32_t num_threads;
    if (value == NULL)
    {
        num_threads = Thread::howManyThreads(25, length);
    }
    else
    {
        num_threads = atoi(value);
    }

    std::cout << "Total number of threads : " << num_threads << std::endl;

    unsigned long const block_size = std::ceil(length / num_threads);

    // std::vector<std::thread> threads(num_threads-1);
    std::vector<std::thread> threads;
    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); i++)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);

        /*
        threads[i] = std::thread(accumalate_block<Iterator, T>(),
                      block_start,
                      block_end,
                      func_);
        */
        accumalate_block<Iterator, T> accumalate;
        // threads.push_back( std::thread(accumalate_block<Iterator, T>(), block_start, block_end, func_ ) );
        threads.push_back(std::thread(std::bind(&accumalate_block<Iterator, T>::perform, accumalate, block_start, block_end, func_)));
        block_start = block_end;
    }

    accumalate_block<Iterator, T>()(block_start, last, func_);

    for (int i = 0; i < (num_threads - 1); i++)
    {
        if (threads[i].joinable())
            threads[i].join();
    }
}

#endif

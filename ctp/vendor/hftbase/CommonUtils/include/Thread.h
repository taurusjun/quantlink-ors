#ifndef _THREAD_H
#define _THREAD_H

#include <thread>
#include <utility>
#include <inttypes.h>
#include <iostream>

class Thread
{
public:
    Thread() = default;

    explicit Thread(std::thread t_) : m_t(std::move(t_))
    {
        if (!m_t.joinable())
            throw std::logic_error("No thread");
    }

    ~Thread()
    {
        m_t.join();
    }

    Thread(Thread const &) = delete;

    static int32_t howManyThreads(int32_t min_per_thread_, int32_t task_size_)
    {
        int32_t max_threads = (task_size_ + min_per_thread_ - 1) / min_per_thread_;
        int32_t hardware_threads = std::thread::hardware_concurrency();
        int32_t num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);

        return num_threads;
    }

    std::thread &getThread()
    {
        return this->m_t;
    }

    Thread &operator=(Thread &_t)
    {
        if (m_t.joinable())
        {
            std::cout << "ERROR: YOU TRYING TO SWAP TO A THREAD THAT HAS DIED" << std::endl;
            std::terminate();
        }
        m_t.swap(_t.getThread());
        return *this;
    }

private:
    std::thread m_t;
};

#endif
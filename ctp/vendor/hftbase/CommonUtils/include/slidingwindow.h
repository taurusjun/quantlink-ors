#include <list>
#include <functional>
#include <iostream>

namespace illuminati
{
    namespace ds
    {

        template <typename T, uint64_t window>
        class sliding_window
        {
        private:
            std::list<std::pair<uint64_t, T>> m_Q;

        public:
            sliding_window(){};
            void push(T val)
            {
                if (m_Q.size() >= window)
                {
                    m_Q.pop();
                }

                m_Q.push_back(std::make_pair(0, val));
            }

            void push(T val, uint64_t key)
            {
                while (!m_Q.empty() && m_Q.front().first < key)
                    m_Q.pop_front();

                m_Q.push_back(std::make_pair(key + window, val));
            }

            typename std::list<std::pair<uint64_t, T>>::iterator begin()
            {
                return m_Q.begin();
            }
            typename std::list<std::pair<uint64_t, T>>::iterator end()
            {
                return m_Q.end();
            }
        };
    }
}

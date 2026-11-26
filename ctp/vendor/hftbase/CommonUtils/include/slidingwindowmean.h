#include <queue>
#include <functional>
#include <iostream>

namespace illuminati
{
    namespace ds
    {

        template <typename T, unsigned int window>
        class sliding_window_mean
        {
        private:
            std::queue<T> m_Q;
            T m_total;

        public:
            sliding_window_mean() : m_total(0){};
            void push(T val)
            {
                if (m_Q.size() >= window)
                {
                    m_total -= m_Q.front();
                    m_Q.pop();
                }

                m_total += val;
                m_Q.push(val);
            }

            double mean()
            {
                return ((double)m_total) / ((double)m_Q.size());
            }
        };

        // dynamic window
        template <typename T>
        class sliding_window_mean<T, 0>
        {
        private:
            std::queue<T> m_Q;
            T m_total;
            unsigned int window;

        public:
            sliding_window_mean() : m_total(0), window(1){};
            sliding_window_mean(unsigned int w) : m_total(0), window(w){};
            void setWindow(unsigned int w) { window = w; };

            void push(T val)
            {
                if (m_Q.size() >= window)
                {
                    m_total -= m_Q.front();
                    m_Q.pop();
                }

                m_total += val;
                m_Q.push(val);
            }

            double mean()
            {
                return ((double)m_total) / ((double)m_Q.size());
            }

            void clear()
            {
                m_total = 0;
                while (!m_Q.empty())
                    m_Q.pop();
            }
        };
    }
}

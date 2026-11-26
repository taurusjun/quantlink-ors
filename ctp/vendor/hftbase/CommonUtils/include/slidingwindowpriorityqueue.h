#include <deque>
#include <functional>

namespace illuminati
{
    namespace ds
    {

        template <typename T, unsigned int window, typename Cmp = std::greater<T>>
        class sliding_window_priority_queue
        {
        private:
            std::deque<std::pair<unsigned int, T>> m_Q;
            unsigned int index;
            Cmp cmp;

        public:
            sliding_window_priority_queue() : m_Q(), index(0), cmp() {}
            sliding_window_priority_queue(Cmp &c) : m_Q(), index(0), cmp(c) {}

            void push(T val)
            {
                // Amortized O(1)
                index++;
                while (!m_Q.empty() && m_Q.front().first <= index)
                    m_Q.pop_front();

                while (!m_Q.empty() && cmp(val, m_Q.back().second))
                {
                    m_Q.pop_back();
                }

                m_Q.push_back(std::make_pair(index + window, val));
            }

            T &top()
            {
                return m_Q.front().second;
            }
            void clear()
            {
                m_Q.clear();
            }
        };

        // specify window dynamically
        template <typename T, typename Cmp>
        class sliding_window_priority_queue<T, 0, Cmp>
        {
        private:
            std::deque<std::pair<unsigned int, T>> m_Q;
            unsigned int index;
            unsigned int window;
            Cmp cmp;

        public:
            sliding_window_priority_queue() : m_Q(), index(0), window(1), cmp() {}
            sliding_window_priority_queue(unsigned int w) : m_Q(), index(0), window(w), cmp() {}
            sliding_window_priority_queue(unsigned int w, Cmp &c) : m_Q(), index(0), window(w), cmp(c) {}

            void setWindow(unsigned int w) { window = w; }

            void push(T val)
            {
                // Amortized O(1)
                index++;
                while (!m_Q.empty() && m_Q.front().first <= index)
                    m_Q.pop_front();

                while (!m_Q.empty() && cmp(val, m_Q.back().second))
                {
                    m_Q.pop_back();
                }

                m_Q.push_back(std::make_pair(index + window, val));
            }

            T &top()
            {
                return m_Q.front().second;
            }

            void clear()
            {
                index = 0;
                m_Q.clear();
            }
        };
    }
}

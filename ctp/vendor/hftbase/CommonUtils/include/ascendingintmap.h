#include <vector>
#include <stdint.h>
namespace illuminati
{
    namespace ds
    {

        template <typename T>
        struct Option
        {
            T data;
            bool valid;

            Option() : valid(false) {}
        };

        template <typename IntType, typename T>
        struct AscendingIntMapIter
        {
            AscendingIntMap<IntType, T> *map;
            IntType index;

            void operator++()
            {
                index++;
                while (index < map->m_maxkey && map->vec[index & m_size - 1].valid == false)
                {
                    index++;
                }
            }

        }

        template <typename IntType, typename T>
        class AscendingIntMap
        {
        protected:
            std::vector<Option<T>> vec;

            IntType m_minkey;
            IntType m_maxkey;
            unsigned int m_size;

            void resize_aux(unsigned int size)
            {
                vec.reserve(size);
                m_size = size;
            }

        public:
            friend AscendingIntMapIter<IntType, T>;

            typedef AscendingIntMapIter<IntType, T> iterator;

            AscendingIntMap() : m_maxkey(0), m_minkey(0)
            {
            }

            iterator begin()
            {
                return AscendingIntMapIter<IntType, T>(this, m_minkey);
            }

            iterator end()
            {
                return AscendingIntMapIter<IntType, T>(this, m_maxkey);
            }

            iterator find(IntType i)
            {
                if (i < m_minkey || (i >= m_maxkey))
                    return vec.end();

                Option<T> *opt = &vec[i & (m_size - 1)];
                if (opt->valid == true)
                {
                    return static_cast<T *>(opt);
                }
                else
                    return vec.end();
            }

            void reserve(unsigned int newsize)
            {
                resize_aux(getMinHighestPowerOf2(newsize));
            }

            bool insert(IntType i, T type)
            {
                if (i < m_maxkey)
                    return false;

                if (m_minkey == m_maxkey)
                    m_minkey = i;
                m_maxkey = i + 1;

                if (m_maxkey - m_minkey >= m_size)
                    resize_aux(m_size * 2);

                Option<T> *opt = &vec[i & (m_size - 1)];
                opt->valid = true;
                opt->data = T;
                return true;
            }

            bool erase(IntType i)
            {
                if (i >= m_maxkey || i < m_minkey)
                {
                    return false;
                }

                vec[i & (m_size - 1)].valid = false;

                if (i == m_minkey)
                {
                    unsigned int j = i + 1;
                    m_minkey = 0;
                    for (; j <= m_maxkey; j++)
                    {
                        if (vec[j & (m_size - 1)].valid == true)
                            m_minkey = j;
                    }
                }
                if (i == m_maxkey - 1)
                {
                    unsigned int j = i - 1;
                    m_maxkey = m_minkey;
                    for (; j >= m_minkey; j++)
                    {
                        if (vec[j & (m_size - 1)].valid == true)
                            m_maxkey = j + 1;
                    }
                }
                return true;
            }

            bool empty()
            {
                return (m_maxkey == m_minkey);
            }
        };
    } // namespace ds
}

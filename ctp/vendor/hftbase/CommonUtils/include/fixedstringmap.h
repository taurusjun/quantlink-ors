#ifndef FIXEDSTRINGMAP_H
#define FIXEDSTRINGMAP_H
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
// #include "cstring_sse.h"

// TODO : currently just a balanced binary tree. But try to use known string stuff to reduce string comparisons
namespace illuminati
{
    namespace ds
    {
        class fixed_string_set
        {
        protected:
            char *m_buffer;
            char **m_array;
            size_t m_size;
            size_t m_bufSize;

            inline __attribute__((always_inline)) unsigned int leftChild(unsigned int i)
            {
                return 2 * i;
            }
            inline __attribute__((always_inline)) unsigned int rightChild(unsigned int i)
            {
                return 2 * i + 1;
            }
            void createTree(std::vector<std::string> &v, int start, int end, unsigned int nodeindex, unsigned int &bufindex)
            {
                if (start > end)
                    return;

                unsigned int mid = start + (end - start) / 2;
                strcpy(m_buffer + bufindex, v[mid].c_str());
                m_array[nodeindex] = m_buffer + bufindex;

                bufindex += v[mid].size() + 1;
                createTree(v, start, mid - 1, leftChild(nodeindex), bufindex);
                createTree(v, mid + 1, end, rightChild(nodeindex), bufindex);
            }

            void printKeys(unsigned int i)
            {
                if (i > 2 * m_size + 1 || m_array[i] == NULL)
                    return;
                printKeys(leftChild(i));
                std::cout << "(" << i << ":" << m_array[i] << "), ";
                printKeys(rightChild(i));
            }

        public:
            typedef char *value_type;
            // template <template <typename> class Container >
            //

            fixed_string_set(const std::vector<std::string> &c)
            {
                m_size = c.size();
                m_bufSize = 0;
                if (m_size == 0)
                {
                    m_array = new value_type[2];
                    for (unsigned int i = 0; i < m_size * 2 + 2; i++)
                    {
                        m_array[i] = NULL;
                    }
                    return;
                }
                std::vector<std::string> tmp;
                tmp.reserve(m_size);
                for (auto it = c.begin(); it != c.end(); it++)
                {
                    m_bufSize += it->length() + 1;
                    tmp.push_back(*it);
                }

                m_buffer = new char[m_bufSize];
                m_array = new value_type[m_size * 2 + 2]; // TODO: compact

                for (unsigned int i = 0; i < m_size * 2 + 2; i++)
                {
                    m_array[i] = NULL;
                }

                std::sort(tmp.begin(), tmp.end());
                unsigned int b = 0;
                createTree(tmp, 0, m_size - 1, 1, b);
            }

            bool empty()
            {
                return (m_size == 0);
            }

            char *find(const char *c)
            {
                unsigned int i = 1;

                while (i <= 2 * m_size + 1)
                {
                    if (m_array[i] == NULL)
                        return NULL;
                    // int res = infra::strcmp_sse_unsafe(c, m_array[i]);
                    int res = strcmp(c, m_array[i]);
                    if (res < 0)
                    {
                        i = leftChild(i);
                    }
                    else if (res > 0)
                    {
                        i = rightChild(i);
                    }
                    else
                    {
                        return (m_array[i]);
                    }
                }
                return NULL;
            }

            void printKeys()
            {
                printKeys(1);
                std::cout << "\n";
            }
        };
    }
}
namespace illuminati
{
    namespace ds
    {

        template <typename T>
        class fixed_string_map
        {
        public:
            struct value_type
            {
                const char *str;
                T val;
            };

        protected:
            char *m_buffer;
            value_type *m_array;
            size_t m_size;
            size_t m_bufSize;

            inline __attribute__((always_inline)) unsigned int leftChild(unsigned int i)
            {
                return 2 * i;
            }
            inline __attribute__((always_inline)) unsigned int rightChild(unsigned int i)
            {
                return 2 * i + 1;
            }

            template <typename StringContainer>
            void createTree(StringContainer &v, int start, int end, unsigned int nodeindex, unsigned int &bufindex)
            {
                if (start > end)
                    return;

                unsigned int mid = start + (end - start) / 2;
                strcpy(m_buffer + bufindex, v[mid].c_str());
                m_array[nodeindex].str = m_buffer + bufindex;

                bufindex += v[mid].size() + 1;
                createTree(v, start, mid - 1, leftChild(nodeindex), bufindex);
                createTree(v, mid + 1, end, rightChild(nodeindex), bufindex);
            }

            void printKeys(unsigned int i)
            {
                if (i > 2 * m_size + 1 || m_array[i].str == NULL)
                    return;
                printKeys(leftChild(i));
                std::cout << "(" << i << ":" << m_array[i].str << "), ";
                printKeys(rightChild(i));
            }

            void getMap(std::map<std::string, T> &m, unsigned int i)
            {
                if (i > 2 * m_size + 1 || m_array[i].str == NULL)
                    return;
                getMap(m, leftChild(i));
                m.insert(std::make_pair(std::string(m_array[i].str), m_array[i].val));
                getMap(m, rightChild(i));
            }

        public:
            size_t size()
            {
                return m_size;
            }

            template <typename StringContainer>
            void create_map(const StringContainer &c)
            {
                m_size = c.size();
                m_bufSize = 0;

                if (m_size == 0)
                {
                    m_array = new value_type[2];
                    for (unsigned int i = 0; i < m_size * 2 + 2; i++)
                    {
                        m_array[i].str = NULL;
                    }
                    return;
                }

                std::vector<std::string> tmp;
                tmp.reserve(m_size);
                for (auto it = c.begin(); it != c.end(); it++)
                {
                    m_bufSize += it->length() + 1;
                    tmp.push_back(*it);
                }
                // m_bufSize += 16;

                m_buffer = new char[m_bufSize];
                m_array = new value_type[m_size * 2 + 2]; // TODO: compact

                for (unsigned int i = 0; i < m_size * 2 + 2; i++)
                {
                    m_array[i].str = NULL;
                }

                std::sort(tmp.begin(), tmp.end());
                unsigned int b = 0;
                createTree(tmp, 0, m_size - 1, 1, b);
            }

            fixed_string_map() : m_size(0)
            {
            }

            value_type *find(const char *c)
            {
                unsigned int i = 1;

                while (i <= 2 * m_size + 1)
                {
                    if (m_array[i].str == NULL)
                        return NULL;
                    //  int res = infra::strcmp_sse_unsafe(c, m_array[i].str);
                    int res = strcmp(c, m_array[i].str);
                    if (res < 0)
                    {
                        i = leftChild(i);
                    }
                    else if (res > 0)
                    {
                        i = rightChild(i);
                    }
                    else
                    {
                        return &(m_array[i]);
                    }
                }
                return NULL;
            }

            std::map<std::string, T> getMap()
            {
                std::map<std::string, T> m;
                getMap(m, 1);
                return m;
            }

            void printKeys()
            {
                printKeys(1);
                std::cout << "\n";
            }
        };

    } // namespace ds

} // namespace illuminati
#endif

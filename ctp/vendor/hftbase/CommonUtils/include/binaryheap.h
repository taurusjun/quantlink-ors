/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#ifndef BINARYHEAP_H
#define BINARYHEAP_H

#include <stdint.h>
#include <vector>

namespace illuminati
{
    namespace ds
    {
        /*!
         * @tparam T Type of the data to be stored.
         * @tparam ComparisonFunc
         * @tparam size Maximum size of the heap.
         */
        template <typename T, typename ComparisonFunc>
        class BinaryHeap
        {
        private:
            std::vector<T> buffer;
            ComparisonFunc comparator;
            unsigned int size;
            __inline __const unsigned int getParentIndex(unsigned int index);
            __inline __const unsigned int getLeftChildIndex(unsigned int index);
            __inline __const unsigned int getRightChildIndex(unsigned int index);
            __inline __const unsigned int getLargestChildIndex(unsigned int index);
            __inline__ void swap(T *left, T *right)
            {
                T temp = *right;
                *right = *left;
                *left = temp;
            }

        public:
            BinaryHeap() : size(0)
            {
                buffer.resize(10);
            }
            void insert(const T &value);
            T removeRoot();
            T removeRootAndInsert(const T &value);
            __inline unsigned int getSize()
            {
                return size;
            }
            __inline bool isEmpty()
            {
                return size == 0;
            }
            __inline T getRoot()
            {
                return buffer[1];
            }

            __inline T *getRootPtr()
            {
                return &buffer[1];
            }
            void print();
        };
    }
}

#endif // BINARYHEAP_H

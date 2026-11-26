#ifndef _INFRA_ARRAYBINARYHEAP_H
#define _INFRA_ARRAYBINARYHEAP_H

#include <stdint.h>
#include <vector>
#include <iostream>
#include <cstddef>

namespace illuminati
{
    namespace ds
    {
        /*!
         * @tparam T Type of the data to be stored.
         * @tparam ComparisonFunc
         * @tparam size Maximum size of the heap.
         */
        template <typename T, typename ComparisonFunc, size_t N>
        class ArrayBinaryHeap
        {
        private:
            T buffer[N ? N : 1];
            ComparisonFunc comparator;
            int size;
            __inline __const int getParentIndex(int index);
            __inline __const int getLeftChildIndex(int index);
            __inline __const int getRightChildIndex(int index);
            __inline __const int getLargestChildIndex(int index);
            __inline __const void swap(T *left, T *right)
            {
                T temp = *right;
                *right = *left;
                *left = temp;
            }

            /*!
             * pre-order traverse the tree and print it on stream
             */
            void __preordertraverse(std::ostream &stream_, int32_t index)
            {
                if (index < size)
                {
                    stream_ << buffer[index] << "\t";
                    __preordertraverse(stream_, getLeftChildIndex(index));
                    __preordertraverse(stream_, getRightChildIndex(index));
                }
            }

        public:
            ArrayBinaryHeap() : size(0) {}
            void insert(T value);
            T removeRootAndInsert(T);
            /*!
             * returns total number of nodes in the heap implemented as a partially ordered binary tree.
             * @return number nodes in the tree
             */
            __inline int getSize()
            {
                return size;
            }
            /*!
             * @return true if there a 0 nodes in the heap else returns true.
             */
            __inline bool isEmpty()
            {
                return size == 0;
            }
            /*!
             * returns the best node in the heap
             * @return best node value type
             */
            __inline T getRoot()
            {
                return buffer[0];
            }
            void print();

            /*!
             * pre-order traversal of the binary heap
             */
            void preorderprint()
            {
                __preordertraverse(std::cout, 0);
                std::cout << std::endl;
            }

            T getBest();
            __inline T heapify();
            T heapifyTree(int32_t);
            int32_t getLowestLevelRightMostLeaf(int32_t, int32_t);
            bool inSameLevel(int32_t, int32_t);
        };
    }
}

#endif // ARRAYBINARYHEAP_H

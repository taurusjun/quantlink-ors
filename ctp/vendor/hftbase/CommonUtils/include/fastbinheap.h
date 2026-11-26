#ifndef FASTBINARYHEAP_H
#define FASTBINARYHEAP_H

#include <stdint.h>
#include <vector>

namespace illuminati
{
    namespace ds
    {
        /*!
         * @tparam T Type of the data to be stored.
         * @tparam ComparisonField : functor that returns field on which to base comparison
         * @tparam U : type of comparison field
         * @tparam size Maximum size of the heap.
         */
        template <typename T, typename ComparisonField, typename U = uint64_t>
        class FastBinHeap
        {
        private:
            struct TPair
            {
                U comp;
                T *ptr;
            };

            T *buffer;
            TPair *heap;
            unsigned int size;
            unsigned int size_;
            unsigned int capacity;

            ComparisonField comp;
            __inline __const unsigned int getParentIndex(unsigned int index);
            __inline __const unsigned int getLeftChildIndex(unsigned int index);
            __inline __const unsigned int getRightChildIndex(unsigned int index);
            __inline __const unsigned int getLargestChildIndex(unsigned int index);

            __inline__ void swap(TPair *left, TPair *right)
            {
                TPair temp = *right;
                *right = *left;
                *left = temp;
            }

        public:
            FastBinHeap(size_t c) : size(0), size_(0), capacity(c + 1)
            {
                buffer = new T[capacity];
                heap = new TPair[capacity];
            }

            ~FastBinHeap()
            {
                delete[] buffer;
                delete[] heap;
            }
            void insert(const T &value);
            T removeRoot();
            void removeRootAndInsert(const T &value);
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
                return *(heap[1].ptr);
            }

            __inline T *getRootPtr()
            {
                return heap[1].ptr;
            }
            void print();
        };
    }
}

#endif // BINARYHEAP_H

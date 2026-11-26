#ifndef _FASTBINHEAP_HPP
#define _FASTBINHEAP_HPP
#include "fastbinheap.h"
#include <math.h>
#include <iostream>
#include <cstring>

#define AT(j) reinterpret_cast<TPair *>(reinterpret_cast<char *>(heap) + j)
using namespace illuminati::ds;

template <typename T, typename ComparisonOp, typename U>
__inline __const unsigned int FastBinHeap<T, ComparisonOp, U>::getParentIndex(unsigned int index)
{
    return (index / (2 * sizeof(TPair))) * sizeof(TPair);
}

template <typename T, typename ComparisonOp, typename U>
__inline __const unsigned int FastBinHeap<T, ComparisonOp, U>::getLeftChildIndex(unsigned int index)
{
    return 2 * index;
}

template <typename T, typename ComparisonOp, typename U>
__inline __const unsigned int FastBinHeap<T, ComparisonOp, U>::getRightChildIndex(unsigned int index)
{
    return 2 * index + sizeof(TPair);
}

template <typename T, typename ComparisonOp, typename U>
__inline __const unsigned int FastBinHeap<T, ComparisonOp, U>::getLargestChildIndex(unsigned int index)
{
    unsigned int left = getLeftChildIndex(index);
    unsigned int right = getRightChildIndex(index);
    unsigned int largestChildIndex = 0;
    /*
    if(left < size && buffer[index] < buffer[left])
      largestChildIndex = left;
    if(right < size && buffer[index] < buffer[right] && (largestChildIndex == -1 || buffer[left] < buffer[right]) )
      largestChildIndex = right;
    return largestChildIndex;
  */
    if (left <= size_ && AT(index)->comp > AT(left)->comp)
        largestChildIndex = left;
    if (right <= size_ && (AT(index)->comp > AT(right)->comp) && (largestChildIndex == 0 || (AT(left)->comp > AT(right)->comp)))
        largestChildIndex = right;
    return largestChildIndex;
}

template <typename T, typename ComparisonOp, typename U>
void FastBinHeap<T, ComparisonOp, U>::insert(const T &value)
{
    size++;
    size_ += sizeof(TPair);
    unsigned int bufindex = size;
    unsigned int index = size_;

    if (size == capacity)
    {
        throw "capacity exceeded\n";
    }

    memcpy((void *)(&(buffer[bufindex])), (void *)(&value), sizeof(T));
    // Add the element to the bottom level of the heap
    AT(index)->comp = comp(value);
    AT(index)->ptr = &(buffer[bufindex]);

    int newIndex = getParentIndex(index);
    while (newIndex > 0)
    {
        // Compare the added element with its parent.
        if (AT(newIndex)->comp > AT(index)->comp)
        {
            // If they are not in the correct order, swap and continue with it.
            swap(AT(index), AT(newIndex));
            index = newIndex;
            newIndex = getParentIndex(index);
        }
        else
        {
            // If they are in order, stop.
            break;
        }
    }
}

template <typename T, typename ComparisonOp, typename U>
T FastBinHeap<T, ComparisonOp, U>::removeRoot()
{
    if (size == 0)
        throw "Heap is empty";

    T root = *(AT(sizeof(TPair))->ptr);

    // Replace the root of the heap with the last element on the last level.
    *(AT(sizeof(TPair))) = *(AT(size_));
    size--;
    size_ -= sizeof(TPair);
    // if(size == 0)
    //   buffer.clear();
    unsigned int index = sizeof(TPair);
    while (index <= size_)
    {
        // int leftChildIndex = getLeftChildIndex(index);
        // int rightChildIndex = getRightChildIndex(index);
        int largestChildIndex = getLargestChildIndex(index);
        // std::cout<<"Remove(): LargetChildIndex="<<largestChildIndex<<"\n";
        // Stop if the new root is greater than any of its children.
        if (largestChildIndex == 0)
            break;

        swap(AT(index), AT(largestChildIndex));
        index = largestChildIndex;
    }
    return root;
}

template <typename T, typename ComparisonOp, typename U>
void FastBinHeap<T, ComparisonOp, U>::removeRootAndInsert(const T &value)
{
    if (size == 0)
        throw "Heap is empty";
    // Replace the root of the heap with the new value.
    *(AT(sizeof(TPair))->ptr) = value;
    AT(sizeof(TPair))->comp = comp(value);

    int index = sizeof(TPair);
    while (index <= size_)
    {
        int largestChildIndex = getLargestChildIndex(index);

        // Stop if the new root is greater than any of its children.
        if (largestChildIndex == 0)
            break;

        swap(AT(index), AT(largestChildIndex));
        index = largestChildIndex;
    }
}

template <typename T, typename ComparisonOp, typename U>
void FastBinHeap<T, ComparisonOp, U>::print()
{
    std::cout << "\n*********\n";
    for (int i = 0; i < size; i++)
    {
        std::cout << buffer[i] << ", ";
    }
    std::cout << "\n";
}
#endif

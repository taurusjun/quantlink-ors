/*
 * Company: way2wealth-illuminati
 * Copyright (c) 2012, All Rights Reserved, http://www.way2wealth-illuminati.com/
 *
 */

#include "binaryheap.h"
#include <math.h>
#include <iostream>

using namespace illuminati::ds;

template <typename T, typename ComparisonFunc>
__inline __const unsigned int BinaryHeap<T, ComparisonFunc>::getParentIndex(unsigned int index)
{
    return index / 2;
}

template <typename T, typename ComparisonFunc>
__inline __const unsigned int BinaryHeap<T, ComparisonFunc>::getLeftChildIndex(unsigned int index)
{
    return 2 * index;
}

template <typename T, typename ComparisonFunc>
__inline __const unsigned int BinaryHeap<T, ComparisonFunc>::getRightChildIndex(unsigned int index)
{
    return 2 * index + 1;
}

template <typename T, typename ComparisonFunc>
__inline __const unsigned int BinaryHeap<T, ComparisonFunc>::getLargestChildIndex(unsigned int index)
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
    if (left <= size && comparator(buffer[index], buffer[left]))
        largestChildIndex = left;
    if (right <= size && comparator(buffer[index], buffer[right]) && (largestChildIndex == 0 || comparator(buffer[left], buffer[right])))
        largestChildIndex = right;
    return largestChildIndex;
}

template <typename T, typename ComparisonFunc>
void BinaryHeap<T, ComparisonFunc>::insert(const T &value)
{
    size++;
    unsigned int index = size;

    // Add the element to the bottom level of the heap.
    if (index >= buffer.size())
        buffer.push_back(value);
    else
        buffer[index] = value;
    // if(size == 0)
    //   buffer[0] = value;
    // else
    //   buffer.push_back(value);

    unsigned int newIndex = getParentIndex(index);
    while (newIndex > 0)
    {
        // Compare the added element with its parent.
        if (comparator(buffer[newIndex], buffer[index]))
        {
            // If they are not in the correct order, swap and continue with it.
            swap(&buffer[index], &buffer[newIndex]);
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

template <typename T, typename ComparisonFunc>
T BinaryHeap<T, ComparisonFunc>::removeRoot()
{
    if (size == 0)
        throw "Heap is empty";
    T root = buffer[1];
    // Replace the root of the heap with the last element on the last level.
    buffer[1] = buffer[size];
    size--;
    // if(size == 0)
    //   buffer.clear();
    unsigned int index = 1;
    while (index <= size)
    {
        // unsigned int leftChildIndex = getLeftChildIndex(index);
        // unsigned int rightChildIndex = getRightChildIndex(index);
        unsigned int largestChildIndex = getLargestChildIndex(index);
        // std::cout<<"Remove(): LargetChildIndex="<<largestChildIndex<<"\n";
        // Stop if the new root is greater than any of its children.
        if (largestChildIndex == 0)
            break;
        // Compare the new root with the largest child
        // if(buffer[index] < buffer[largestChildIndex])
        if (comparator(buffer[index], buffer[largestChildIndex]))
        {
            // std::cout<<"Remove(): swapping index and largestChildIndex\n";
            swap(&buffer[index], &buffer[largestChildIndex]);
            index = largestChildIndex;
        }
    }
    return root;
}

template <typename T, typename ComparisonFunc>
T BinaryHeap<T, ComparisonFunc>::removeRootAndInsert(const T &value)
{
    if (size == 0)
        throw "Heap is empty";
    T root = buffer[1];
    // Replace the root of the heap with the new value.
    buffer[1] = value;
    unsigned int index = 1;
    while (index <= size)
    {
        // unsigned int leftChildIndex = getLeftChildIndex(index);
        // unsigned int rightChildIndex = getRightChildIndex(index);
        unsigned int largestChildIndex = getLargestChildIndex(index);
        // std::cout<<"Remove(): LargetChildIndex="<<largestChildIndex<<"\n";
        // Stop if the new root is greater than any of its children.
        if (largestChildIndex == 0)
            break;
        // Compare the new root with the largest child

        // if(comparator(buffer[index], buffer[largestChildIndex]))
        // {
        // std::cout<<"Remove(): swapping index and largestChildIndex\n";
        swap(&buffer[index], &buffer[largestChildIndex]);
        index = largestChildIndex;
        //}
    }
    return root;
}

template <typename T, typename ComparisonFunc>
void BinaryHeap<T, ComparisonFunc>::print()
{
    std::cout << "\n*********\n";
    for (unsigned int i = 0; i < size; i++)
    {
        std::cout << buffer[i] << ", ";
    }
    std::cout << "\n";
}

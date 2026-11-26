#include "arraybinaryheap.h"
#include <math.h>
#include <iostream>

using namespace illuminati::ds;

/*!
 * returns parent of a particular node
 * @param index index of a particular node
 * @return index of the parent node
 */
template <typename T, typename ComparisonFunc, size_t N>
__inline __const int32_t ArrayBinaryHeap<T, ComparisonFunc, N>::getParentIndex(int32_t index)
{
    return floor((index - 1) / 2.0);
}

/*!
 * returns left child of a given node
 * @param index index of a particular node
 * @return index of the left child
 */
template <typename T, typename ComparisonFunc, size_t N>
__inline __const int32_t ArrayBinaryHeap<T, ComparisonFunc, N>::getLeftChildIndex(int32_t index)
{
    return 2 * index + 1;
}

/*!
 * returns right child of a given node
 * @param index index of a particular node
 * @return index of the right child
 */
template <typename T, typename ComparisonFunc, size_t N>
__inline __const int32_t ArrayBinaryHeap<T, ComparisonFunc, N>::getRightChildIndex(int32_t index)
{
    return 2 * index + 2;
}

/*!
 * inserts the new node along with addition of a new node
 * @param value value type
 * @return value type which is the best node in the heap
 */
template <typename T, typename ComparisonFunc, size_t N>
T ArrayBinaryHeap<T, ComparisonFunc, N>::removeRootAndInsert(T value)
{
    T best = buffer[0];
    buffer[0] = value;
    heapifyTree(0);
    return best;
}

/*!
 * removes the root and re-balances the tree
 * @return value type which is the best node in the heap
 */
template <typename T, typename ComparisonFunc, size_t N>
__inline T ArrayBinaryHeap<T, ComparisonFunc, N>::heapify()
{
    return heapifyTree(0);
}

/*!
 * insert a new node and balances the tree
 * @param value value type to be added
 */
template <typename T, typename ComparisonFunc, size_t N>
void ArrayBinaryHeap<T, ComparisonFunc, N>::insert(T value)
{
    int32_t index = size;
    size++;
    // Add the element to the bottom level of the heap.
    if (index > (N - 1))
    {
        throw std::string("binary heap no more memory available");
    }

    buffer[index] = value;

    int32_t newIndex = getParentIndex(index);
    while (newIndex >= 0)
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

/*!
 * visualization function to print the tree in the way it is stored physically in an array
 */
template <typename T, typename ComparisonFunc, size_t N>
void ArrayBinaryHeap<T, ComparisonFunc, N>::print()
{
    for (int32_t i = 0; i < size; i++)
    {
        std::cout << "[" << i << "]" << buffer[i] << ", ";
    }
    std::cout << "\n";
}

/*!
 * checks if two nodes in the tree are at the same level. usefull for level order traversal of tree. implementation
 * is specific to array based implementation of the tree
 * @param x given node
 * @param y given node
 * @return bool value - true if the nodes are at the same level
 */
template <typename T, typename ComparisonFunc, size_t N>
bool ArrayBinaryHeap<T, ComparisonFunc, N>::inSameLevel(int32_t x, int32_t y)
{
    return ((x / 2) >= (y / 2)) ? true : false;
}

/*!
 * for a binary heap, returns the lowest level right most node, helpfull in heapify function based on non-array based
 * implementation of a tree.
 * @param rootindex index of the root of the tree
 * @param last index of the last node of the tree
 * @return index of the lowest level right most child
 */
template <typename T, typename ComparisonFunc, size_t N>
int32_t ArrayBinaryHeap<T, ComparisonFunc, N>::getLowestLevelRightMostLeaf(int32_t rootindex, int32_t last)
{
    int32_t parentindex = 999;
    int32_t possiblenode = last;
    while (parentindex > 0)
    {
        parentindex = getParentIndex(possiblenode);
        if (parentindex == rootindex)
        {
            return last;
        }
        else
        {
            possiblenode = parentindex;
        }
    }

    if (inSameLevel(last, last - 1))
    {
        getLowestLevelRightMostLeaf(rootindex, last - 1);
    }
    else
    {
        return -1;
    }
}

/*!
 * given a tree this balances the tree as per the priority function provided
 * @param root root of the tree
 * @return value type which is the best node in the heap
 */
template <typename T, typename ComparisonFunc, size_t N>
T ArrayBinaryHeap<T, ComparisonFunc, N>::heapifyTree(int32_t root)
{
    int32_t i, j, last;
    if (size == 0)
    {
        throw std::string("queue is empty");
    }
    else
    {
        T minimum = buffer[root]; // make minimum point to the first element, the root
        last = size - 1;
        buffer[root] = buffer[last]; // set the last element as root
        size = size - 1;
        i = root;
        int32_t iterations = 1;
        while (i <= getParentIndex(last))
        {
            if (comparator(buffer[getRightChildIndex(i)], buffer[getLeftChildIndex(i)]) || getLeftChildIndex(i) == last)
            {
                j = getLeftChildIndex(i);
                // 				std::cout << "iterations = " << iterations << ", selected the left child = " << buffer[j] << std::endl;
                iterations++;
            }
            else
            {
                j = getRightChildIndex(i);
                // 				std::cout << "iterations = " << iterations << ", selected the right child = " << buffer[j] << std::endl;
                iterations++;
            }

            if (comparator(buffer[i], buffer[j])) // if child at j is better than parent at i
            {
                swap(&buffer[i], &buffer[j]);
                i = j;
            }
            else
            {
                return minimum;
            }
        }
        return minimum;
    }
}
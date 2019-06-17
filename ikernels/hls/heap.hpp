//
// Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef HEAP_HPP
#define HEAP_HPP

#include <string>
#include <sstream>
#include <iterator>

// A min-heap implementation using a statically allocated array.
// Based on: https://github.com/alyssaq/heap

template <typename T, unsigned Size = 100>
class heap {
    T _data[Size];
    unsigned int _size;

    // Called by insert()
    // If the parent's value is greater than current
    //  the current item bubbles up till we reach root
    unsigned int bubbleUp(unsigned int idx) {
        bool doneBubble = false;

        while (!doneBubble) {
            int parentIdx = parent(idx);
            if (parentIdx == -1) return idx; //base case: root of heap

            if (_data[parentIdx] > _data[idx]) {
                std::swap(_data[parentIdx], _data[idx]);
                idx = parentIdx;
            } else {
                doneBubble = true;
            }
        }

        return idx;
    }

    // Called by delete()
    // If the current is greater than children
    //  the current item bubbles down till no more children
    unsigned int bubbleDown(unsigned int idx) {
        bool doneBubble = false;

        while (!doneBubble) {
            int childIdx = child(idx);
            if (childIdx == -1) return idx; //base case: no children left
            int minIdx = getMinIdx(idx, childIdx, childIdx + 1);

            if (minIdx != idx) {
                std::swap(_data[minIdx], _data[idx]);
                idx = minIdx;
            } else {
                doneBubble = true;
            }
        }

        return idx;
    }

    // Used by bubbleDown()
    //  Get the index of the min value
    //  between 3 given indices to the heap array.
    int getMinIdx(unsigned int aIdx, unsigned int bIdx, unsigned int cIdx) {
        bool isLeftSmaller = (_data[aIdx] < _data[bIdx]);

        if (cIdx >= _size) { //the last right child doesnt exist
            return isLeftSmaller ? aIdx : bIdx;
        } else if (isLeftSmaller) {
            return (_data[aIdx] < _data[cIdx]) ? aIdx : cIdx;
        } else {
            return (_data[bIdx] < _data[cIdx]) ? bIdx : cIdx;
        }
    }

public:
    heap() : _size(0) {}

    ~heap() {};

    bool isEmpty() const {
        return _size == 0;
    }

    int size() const {
        return _size;
    }

    T& getItem(unsigned int idx) {
        return _data[idx];
    }

    int parent(unsigned int idx) const {
        if (_size <= 1) return -1; //empty or root has no parent
        return ((int) (idx -1) / 2); //floor (idx-1/2)
    }

    int child(unsigned int idx) const {
        if (_size <= 1 || 2 * idx + 1 >= _size) return -1; //empty or root has no child
        return (2 * idx + 1);
    }

    unsigned int insert(T val) {
        if (_size >= Size) {
            _data[0] = val;
            return bubbleDown(0);
        }

        _data[_size++] = val;
        return bubbleUp(_size-1);
    }

    unsigned int update(unsigned int idx, T newVal) {
        T oldVal = _data[idx];
        _data[idx] = newVal;
        
        return oldVal < newVal ? bubbleDown(idx) : bubbleUp(idx);
    }

    T extractMin() {
        T min = _data[0];
        _data[0] = _data[_size-1];
        --_size;
        bubbleDown(0);
        return min;
    }

    T& peek() {
        return _data[0];
    }

    std::string toString() const {
        if (isEmpty()) return "";

        std::ostringstream sstream;
        std::copy(&_data[0], &_data[_size - 1], std::ostream_iterator<int>(sstream, " "));
        sstream << _data[_size - 1];

        return sstream.str();
    }
};

#endif //HEAP_HPP

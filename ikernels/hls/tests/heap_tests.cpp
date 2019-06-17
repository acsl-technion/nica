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

#include "heap.hpp"
#include "gtest/gtest.h"

namespace {

    class HeapTest : public testing::Test {
    protected:
        // SetUp() is run immediately before a test starts.
        virtual void SetUp() {
            _oneItemHeap.insert(1);
            _twoItemHeap.insert(1);
            _twoItemHeap.insert(3);
            _heap.insert(1);
            _heap.insert(7);
            _heap.insert(10);
            _heap.insert(3);
            _heap.insert(2);
        }

        // TearDown() is invoked immediately after a test finishes.
        virtual void TearDown() {
        }

        heap<int> _heap;
        heap<int> _oneItemHeap;
        heap<int> _twoItemHeap;
    };

    TEST_F(HeapTest, isEmpty) {
        heap<int> heap;
        EXPECT_TRUE(heap.isEmpty());
        EXPECT_EQ("", heap.toString());
        EXPECT_FALSE(_oneItemHeap.isEmpty());
    }

    TEST_F(HeapTest, size) {
        heap<int> heap;
        EXPECT_EQ(0, heap.size());
        EXPECT_EQ(1, _oneItemHeap.size());
        EXPECT_EQ(2, _twoItemHeap.size());
        EXPECT_EQ(5, _heap.size());
    }

    TEST_F(HeapTest, getItem) {
        EXPECT_EQ(1, _heap.getItem(0));
        EXPECT_EQ(3, _heap.getItem(_heap.size()-1));
    }

    TEST_F(HeapTest, parent) {
        EXPECT_EQ(-1, _oneItemHeap.parent(1));
        EXPECT_EQ(0, _twoItemHeap.parent(2));
        EXPECT_EQ(1, _heap.parent(4));
    }

    TEST_F(HeapTest, child) {
        EXPECT_EQ(-1, _oneItemHeap.child(1));
        EXPECT_EQ(1, _twoItemHeap.child(0));
        EXPECT_EQ(-1, _heap.child(3));
        EXPECT_EQ(3, _heap.child(1));
    }

    TEST_F(HeapTest, insert) {
        EXPECT_TRUE(_heap.getItem(_heap.parent(2)) < _heap.getItem(2));
        EXPECT_TRUE(_heap.getItem(_heap.parent(4)) < _heap.getItem(4));
        EXPECT_EQ("1", _oneItemHeap.toString());
        EXPECT_EQ("1 3", _twoItemHeap.toString());
        EXPECT_EQ("1 2 10 7 3", _heap.toString());
    }

    TEST_F(HeapTest, insert_return_index) {
        heap<int> heap;

        EXPECT_EQ(heap.insert(0), 0);
        EXPECT_EQ(heap.insert(1), 1);
        EXPECT_EQ(heap.insert(-100), 0);
    }


    TEST_F(HeapTest, extractMin) {
        EXPECT_EQ(1, _heap.extractMin());
        EXPECT_EQ(4, _heap.size());
    }

    TEST_F(HeapTest, heapSort) {
        int last_element = std::numeric_limits<int>::min();

        while (!_heap.isEmpty()) {
            int next_element = _heap.extractMin();
            EXPECT_TRUE(next_element > last_element);
            last_element = next_element;
        }
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

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

#include "cms.hpp"
#include "gtest/gtest.h"

namespace {

    // The fixture for testing class CMS.
    class cms_tests : public ::testing::Test {
    protected:
        int hashes[DEPTH][2];
        CountMinSketch sketch;

        cms_tests() : hashes() {}
        
        virtual ~cms_tests() {}

        virtual void SetUp() {
            CountMinSketch::generateHashes(hashes);
            sketch = CountMinSketch();
            sketch.setHashes(hashes);
        }

        virtual void TearDown() {
        }

    };
 

    TEST_F(cms_tests, EstimateIntegers) {
        sketch.update(1, 3);
        sketch.update(2, 2);
        sketch.update(3, 1);
        sketch.update(1, 1);

        EXPECT_EQ(sketch.estimate(1), 3 + 1);
        EXPECT_EQ(sketch.estimate(2), 2);
        EXPECT_EQ(sketch.estimate(3), 1);
        EXPECT_EQ(sketch.estimate(29), 0);
    }

    TEST_F(cms_tests, EstimateStrings) {
        sketch.update("hello", 1);
        sketch.update("world", 2);
        sketch.update("hello", 4);
        sketch.update(",", 29);
        sketch.update("!", 2);

        EXPECT_EQ(sketch.estimate("hello"), 1 + 4);
        EXPECT_EQ(sketch.estimate("world"), 2);
        EXPECT_EQ(sketch.estimate(","), 29);
        EXPECT_EQ(sketch.estimate("!"), 2);
        EXPECT_EQ(sketch.estimate("ikernel"), 0);
    }

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


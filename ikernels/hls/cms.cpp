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

# include <limits>

#include "cms.hpp"

/**
   Class definition for CountMinSketch.
   public operations:
   // overloaded updates
   void update(int item, int c);
   void update(char *item, int c);
   // overloaded estimates
   unsigned int estimate(int item);
   unsigned int estimate(char *item);
**/

void CountMinSketch::generateHashes(int hashes[DEPTH][2]) {
    for (int i = 0; i < DEPTH; ++i) {
        for (int j = 0; j < 2; ++j) {
            hashes[i][j] = static_cast<int>(static_cast<float>(rand()) * static_cast<float>(LONG_PRIME)
                                            / static_cast<float>(RAND_MAX) + 1);

        }
    }
}

int CountMinSketch::getCell(const int row, const int col) const {
    return C[row][col];
}

// CountMinSketch setHashes
void CountMinSketch::setHashes(int hashes[DEPTH][2]) {
    for (int i = 0; i < DEPTH; ++i) {
#pragma HLS unroll
        for (int j = 0; j < 2; ++j) {
#pragma HLS unroll
            this->hashes[i][j] = hashes[i][j];
        }
    }
}

// CountMinSketch constructor
CountMinSketch::CountMinSketch() : total(0), C() {
}

// CountMinSketch totalcount returns the
// total count of all items in the sketch
unsigned int CountMinSketch::totalcount() {
    return total;
}

// countMinSketch update item count (int)
void CountMinSketch::update(int item, int c) {
    total = total + c;
    unsigned int hashval = 0;
    for (unsigned int j = 0; j < DEPTH; j++) {
#pragma HLS unroll
        hashval = (hashes[j][0]*static_cast<unsigned int>(item)+hashes[j][1])%WIDTH;
        C[j][hashval] = C[j][hashval] + c;
    }
}

void CountMinSketch::updateByAddress(int address, int value)
{
    hashes[address/2][address%2] = value;
}

// countMinSketch update item count (string)
void CountMinSketch::update(const char *str, int c) {
    unsigned int hashval = hashstr(str);
    update(hashval, c);
}

// CountMinSketch estimate item count (int)
unsigned int CountMinSketch::estimate(int item) {
    int minval = std::numeric_limits<int>::max();
    unsigned int hashval = 0;
    for (unsigned int j = 0; j < DEPTH; j++) {
        hashval = (hashes[j][0]*static_cast<unsigned int>(item)+hashes[j][1])%WIDTH;
        minval = std::min(minval, C[j][hashval]);
    }
    return minval;
}

// CountMinSketch estimate item count (string)
unsigned int CountMinSketch::estimate(const char *str) {
    int hashval = hashstr(str);
    return estimate(hashval);
}

// generates a hash value for a sting
// same as djb2 hash function
unsigned int CountMinSketch::hashstr(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

CountMinSketch::~CountMinSketch() {}

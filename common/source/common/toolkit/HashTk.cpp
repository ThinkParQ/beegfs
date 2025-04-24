// Paul Hsiehs hash function, available under http://www.azillionmonkeys.com/qed/hash.html


// Paul Hsieh OLD BSD license

// Copyright (c) 2010, Paul Hsieh All rights reserved.

// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:

//     Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.

//     Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials provided
//     with the distribution.

//     Neither my name, Paul Hsieh, nor the names of any other contributors to the code use may not
//     be used to endorse or promote products derived from this software without specific prior
//     written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "common/Common.h"
#include "common/toolkit/hash_library/sha256.h"

#include "HashTk.h"


#define get16bits(d) (*((const uint16_t *) (d)))

uint32_t HashTk::hsieh32(const char* data, int len)
{
   uint32_t hash = len, tmp;
   int rem;

   if(unlikely(len <= 0 || data == NULL) )
      return 0;

   rem = len & 3;
   len >>= 2;

   /* Main loop */
   for(; len > 0; len--)
   {
      hash += get16bits(data);
      tmp = (get16bits(data+2) << 11) ^ hash;
      hash = (hash << 16) ^ tmp;
      data += 2 * sizeof(uint16_t);
      hash += hash >> 11;
   }

   /* Handle end cases */
   switch(rem)
   {
      case 3:
         hash += get16bits(data);
         hash ^= hash << 16;
         hash ^= data[sizeof(uint16_t)] << 18;
         hash += hash >> 11;
         break;
      case 2:
         hash += get16bits(data);
         hash ^= hash << 11;
         hash += hash >> 17;
         break;
      case 1:
         hash += *data;
         hash ^= hash << 10;
         hash += hash >> 1;
   }

   /* Force "avalanching" of final 127 bits */
   hash ^= hash << 3;
   hash += hash >> 5;
   hash ^= hash << 4;
   hash += hash >> 17;
   hash ^= hash << 25;
   hash += hash >> 6;

   return hash;
}

// Generates sha256 hash from the input data and returns the authentication secret containing the 8
// most significant bytes of the hash in little endian order. Matches the behavior of other
// implementations.
uint64_t HashTk::authHash(const unsigned char* data, size_t len)
{
   unsigned char buf[SHA256::HashBytes];
   SHA256 h;
   h.add(data, len);
   h.getHash(buf);

   uint64_t res = 0;
   for (int i = 7; i >= 0; --i)
   {
      res <<= 8;
      res += buf[i];
   }

   return res;
}

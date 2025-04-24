#ifndef BUFFERTK_H_
#define BUFFERTK_H_

#include <common/Common.h>

enum HashTkHashTypes;
typedef enum HashTkHashTypes HashTkHashTypes;


extern uint32_t HashTk_hash32(HashTkHashTypes hashType, const char* data, int len);
extern uint64_t HashTk_hash64(HashTkHashTypes hashType, const char* data, int len);
static inline uint64_t HashTk_hash(HashTkHashTypes hashType, size_t hashSize,
   const char* data, int len);

int HashTk_sha256(const unsigned char* data, unsigned dataLen, unsigned char* outHash);
int HashTk_authHash(const unsigned char* data, unsigned dataLen, uint64_t* outHash);

enum HashTkHashTypes
{
   HASHTK_HSIEHHASH32      = 0,
   HASHTK_HALFMD4,         // as used by ext4
};

/**
 * Caller decides if it wants a 32- or 64-bit hash
 */
uint64_t HashTk_hash(HashTkHashTypes hashType, size_t hashSize, const char* data, int len)
{
   if (hashSize == 64)
      return HashTk_hash64(hashType, data, len);
   else
      return HashTk_hash32(hashType, data, len);
}


#endif /* BUFFERTK_H_ */

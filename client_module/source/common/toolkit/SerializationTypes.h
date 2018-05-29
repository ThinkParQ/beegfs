#ifndef common_toolkit_SerializationTypes_h_uxGn0NqFVyO6ma2TYHAEI
#define common_toolkit_SerializationTypes_h_uxGn0NqFVyO6ma2TYHAEI

#include <common/Common.h>

typedef struct SerializeCtx {
   char* const data;
   unsigned length;
} SerializeCtx;

typedef struct DeserializeCtx {
   const char* data;
   size_t length;
} DeserializeCtx;

typedef struct RawList {
   const char* data;
   unsigned length;
   unsigned elemCount;
} RawList;

#endif

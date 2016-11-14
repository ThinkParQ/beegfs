#include <common/toolkit/serialization/Serialization.h>
#include "ChunkFileInfo.h"

bool ChunkFileInfo::operator==(const ChunkFileInfo& other) const
{
   return dynAttribs == other.dynAttribs
      && chunkSize == other.chunkSize
      && chunkSizeLog2 == other.chunkSizeLog2
      && numCompleteChunks == other.numCompleteChunks;
}

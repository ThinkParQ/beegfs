#ifndef USEDTARGET_H_
#define USEDTARGET_H_

#include <common/fsck/FsckTargetID.h>

namespace db {

struct UsedTarget
{
   uint32_t id;
   uint32_t type;

   typedef std::pair<uint32_t, uint32_t> KeyType;

   KeyType pkey() const { return std::make_pair(id, type); }

   operator FsckTargetID() const
   {
      return FsckTargetID(id, FsckTargetIDType(type) );
   }
};

}

#endif

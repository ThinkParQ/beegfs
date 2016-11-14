#ifndef GETSTORAGETARGETINFOMSGEX_H
#define GETSTORAGETARGETINFOMSGEX_H

#include <common/net/message/storage/GetStorageTargetInfoMsg.h>
#include <common/nodes/TargetStateStore.h>

class GetStorageTargetInfoMsgEx : public GetStorageTargetInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void getStatInfo(int64_t* outSizeTotal, int64_t* outSizeFree, int64_t* outInodesTotal,
         int64_t* outInodesFree, TargetConsistencyState& nodeState);
};


#endif /*GETSTORAGETARGETINFOMSGEX_H*/

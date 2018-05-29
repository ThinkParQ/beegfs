#ifndef ADJUSTCHUNKPERMISSIONSMSGEX_H
#define ADJUSTCHUNKPERMISSIONSMSGEX_H

#include <common/net/message/fsck/AdjustChunkPermissionsMsg.h>
#include <common/net/message/fsck/AdjustChunkPermissionsRespMsg.h>
#include <common/storage/PathInfo.h>
#include <common/storage/striping/StripePattern.h>

class AdjustChunkPermissionsMsgEx : public AdjustChunkPermissionsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      bool sendSetAttrMsg(const std::string& entryID, unsigned userID, unsigned groupID,
         PathInfo* pathInfo, StripePattern* pattern);
};

#endif /*ADJUSTCHUNKPERMISSIONSMSGEX_H*/

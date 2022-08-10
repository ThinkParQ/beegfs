#ifndef READLOCALFILERDMAMSG_H_
#define READLOCALFILERDMAMSG_H_

#ifdef BEEGFS_NVFS
#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RdmaInfo.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/session/rw/ReadLocalFileV2Msg.h>


class ReadLocalFileRDMAMsg : public ReadLocalFileV2MsgBase, public NetMessageSerdes<ReadLocalFileRDMAMsg>
{
   public:
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      ReadLocalFileRDMAMsg(NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID,
         PathInfo* pathInfoPtr, unsigned accessFlags, int64_t offset, int64_t count) :
         ReadLocalFileV2MsgBase(clientNumID, fileHandleID, targetID, pathInfoPtr, accessFlags,
            offset, count),
         BaseType(NETMSGTYPE_ReadLocalFileRDMA) {}

      /**
       * For deserialization only!
       */
      ReadLocalFileRDMAMsg() :
         ReadLocalFileV2MsgBase(),
         BaseType(NETMSGTYPE_ReadLocalFileRDMA) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ReadLocalFileV2MsgBase::serialize(obj, ctx);

         ctx
            % obj->rdmaInfo;
      }


   private:
      // for <buf,len> info
      RdmaInfo rdmaInfo;

   public:
      inline RdmaInfo* getRdmaInfo()
      {
         return &rdmaInfo;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return ReadLocalFileV2MsgBase::getSupportedHeaderFeatureFlagsMask();
      }

      bool isMsgValid() const
      {
         return rdmaInfo.isValid();
      }
};
#endif /* BEEGFS_NVFS */

#endif /*READLOCALFILERDMAMSG_H_*/

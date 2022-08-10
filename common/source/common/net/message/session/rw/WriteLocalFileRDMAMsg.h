#ifndef WRITELOCALFILERDMAMSG_H_
#define WRITELOCALFILERDMAMSG_H_

#ifdef BEEGFS_NVFS
#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RdmaInfo.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/session/rw/WriteLocalFileMsg.h>

class WriteLocalFileRDMAMsg : public WriteLocalFileMsgBase, public NetMessageSerdes<WriteLocalFileRDMAMsg>
{
   public:
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      WriteLocalFileRDMAMsg(const NumNodeID clientNumID, const char* fileHandleID,
         const uint16_t targetID, const PathInfo* pathInfo, const unsigned accessFlags,
         const int64_t offset, const int64_t count) :
         WriteLocalFileMsgBase(clientNumID, fileHandleID, targetID, pathInfo, accessFlags, offset, count),
         BaseType(NETMSGTYPE_WriteLocalFileRDMA) {}

      /**
       * For deserialization only!
       */
      WriteLocalFileRDMAMsg() :
         WriteLocalFileMsgBase(),
         BaseType(NETMSGTYPE_WriteLocalFileRDMA) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         WriteLocalFileMsgBase::serialize(obj, ctx);

         ctx
            % obj->rdmaInfo;
      }

   private:
      RdmaInfo rdmaInfo;

   public:
      inline RdmaInfo* getRdmaInfo()
      {
         return &rdmaInfo;
      }

      void setUserdataForQuota(unsigned userID, unsigned groupID)
      {
         addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_USE_QUOTA);
         WriteLocalFileMsgBase::setUserdataForQuota(userID, groupID);
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return WriteLocalFileMsgBase::getSupportedHeaderFeatureFlagsMask();
      }

      bool isMsgValid() const
      {
         return rdmaInfo.isValid();
      }
};
#endif /* BEEGFS_NVFS */

#endif /*WRITELOCALFILERDMAMSG_H_*/

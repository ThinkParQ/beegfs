#ifndef WRITELOCALFILERDMAMSG_H_
#define WRITELOCALFILERDMAMSG_H_

#ifdef BEEGFS_NVFS
#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RdmaInfo.h>
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
         BaseType(NETMSGTYPE_WriteLocalFileRDMA)
      {
         this->rdmaInfo.initialize();
      }

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
            % obj->rdmaInfo.nents
            % obj->rdmaInfo.tag
            % obj->rdmaInfo.key;

         if ((0 < obj->rdmaInfo.nents) && (obj->rdmaInfo.nents < RDMA_MAX_DMA_COUNT))
         {
            for (size_t i = 0; i < obj->rdmaInfo.nents; i++)
            {
               ctx
                  % obj->rdmaInfo.dma_address[i]
                  % obj->rdmaInfo.dma_length[i]
                  % obj->rdmaInfo.dma_offset[i];
            }
         }
      }

   private:
      RdmaInfo rdmaInfo;

   public:

      RdmaInfo* getRdmaInfo()
      {
         return &rdmaInfo;
      }

      // getSupportedHeaderFeatureFlagsMask() must be defined in WriteLocalFileRDMAMsg
      // for the NetMessageSerdes registration to work.
      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return WRITELOCALFILEMSG_FLAG_SESSION_CHECK | WRITELOCALFILEMSG_FLAG_USE_QUOTA |
            WRITELOCALFILEMSG_FLAG_DISABLE_IO | WRITELOCALFILEMSG_FLAG_BUDDYMIRROR |
            WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND | WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD;
      }

};
#endif /* BEEGFS_NVFS */

#endif /*WRITELOCALFILERDMAMSG_H_*/

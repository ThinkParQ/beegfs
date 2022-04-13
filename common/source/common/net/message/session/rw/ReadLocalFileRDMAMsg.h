#ifndef READLOCALFILERDMAMSG_H_
#define READLOCALFILERDMAMSG_H_

#ifdef BEEGFS_NVFS
#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RdmaInfo.h>
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
         BaseType(NETMSGTYPE_ReadLocalFileRDMA)
      {
         this->rdmaInfo.initialize();
      }

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
         ctx % obj->rdmaInfo.nents;
         if ((0 < obj->rdmaInfo.nents) && (obj->rdmaInfo.nents < RDMA_MAX_DMA_COUNT))
         {
            ctx
               % obj->rdmaInfo.tag
               % obj->rdmaInfo.key;

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
      // for <buf,len> info
      RdmaInfo rdmaInfo;

   public:

      RdmaInfo* getRdmaInfo()
      {
         return &rdmaInfo;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return READLOCALFILEMSG_FLAG_SESSION_CHECK | READLOCALFILEMSG_FLAG_DISABLE_IO |
            READLOCALFILEMSG_FLAG_BUDDYMIRROR | READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND;
      }

};
#endif /* BEEGFS_NVFS */

#endif /*READLOCALFILERDMAMSG_H_*/

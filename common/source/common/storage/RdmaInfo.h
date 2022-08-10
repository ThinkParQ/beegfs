#ifndef _RDMAINFO_H_
#define _RDMAINFO_H_

#ifdef BEEGFS_NVFS

#define	RDMA_MAX_DMA_COUNT	64

/**
 * RdmaInfo contains the information supplied by the client to specify
 * information required to RDMA read/write its buffers. Public members variables
 * are initialized by RDMA read/write message serialization.
 *
 * This class implements an iterator pattern, which allows stateful consumption
 * of the buffer information.
 */
class RdmaInfo
{
   public:
      // these are public to support message serialization
      size_t	count;
      int32_t	tag;
      uint64_t	key;
      uint64_t	dmaAddress[RDMA_MAX_DMA_COUNT];
      uint64_t	dmaLength[RDMA_MAX_DMA_COUNT];
      uint64_t	dmaOffset[RDMA_MAX_DMA_COUNT];
      int      status;

   private:
      size_t   cur;

   public:
      RdmaInfo()
      {
         status = 0;
         count = 0;
         cur = 0;
         tag = 0;
         key = 0;
         memset(dmaAddress, 0, sizeof(dmaAddress));
         memset(dmaLength, 0, sizeof(dmaLength));
         memset(dmaOffset, 0, sizeof(dmaOffset));
      }

      /**
       * Returns true if there are more buffers to iterate.
       */
      inline bool more() const
      {
         return count > cur;
      }

      /**
       * Retrieve information to access a remote buffer and advance the buffer
       * iterator.
       *
       * Returns true if there is a buffer to iterate. False indicates that the
       * set of buffers has been exhausted.
       *
       * @param addr initialized to the remote buffer's address
       * @param len initialized to the remote buffer's length
       * @param offset initialized to the remote buffer's offset
       */
      inline bool next(uint64_t& addr, uint64_t& len, uint64_t& offset)
      {
         if (!more())
            return false;
         addr = dmaAddress[cur];
         len = dmaLength[cur];
         offset = dmaOffset[cur];
         cur++;
         return true;
      }

      /**
       * Returns true if the RDMA information is valid, i.e. buffer count is sane.
       */
      inline bool isValid() const
      {
         return count < RDMA_MAX_DMA_COUNT;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->count
            % obj->tag
            % obj->key;

         if (!obj->isValid())
         {
            LogContext("RdmaInfo").log(Log_ERR,
               "Received a request for " + StringTk::uint64ToStr(obj->count) +
               " buffers, maximum is " + StringTk::intToStr(RDMA_MAX_DMA_COUNT));
            return;
         }

         for (size_t i = 0; i < obj->count; i++)
         {
            ctx
               % obj->dmaAddress[i]
               % obj->dmaLength[i]
               % obj->dmaOffset[i];
         }
      }
};
#endif /* BEEGFS_NVFS */

#endif /* _RDMAINFO_H_ */

#ifndef _RDMAINFO_H_
#define _RDMAINFO_H_

#ifdef BEEGFS_NVFS

#define	RDMA_MAX_DMA_COUNT	64

class RdmaInfo
{

   // these are public to support message serialization
   public:
      size_t	nents;
      int32_t	tag;
      uint64_t	key;
      uint64_t	dma_address[RDMA_MAX_DMA_COUNT];
      uint64_t	dma_length[RDMA_MAX_DMA_COUNT];
      uint64_t	dma_offset[RDMA_MAX_DMA_COUNT];

   private:
      size_t   curEnt;

   public:
      RdmaInfo()
      {
		   initialize();
      }

      void initialize()
      {
         nents = 0;
         curEnt = 0;
         tag = 0;
         key = 0;
         memset(dma_address, 0, sizeof(dma_address));
         memset(dma_length, 0, sizeof(dma_length));
         memset(dma_offset, 0, sizeof(dma_offset));
      }

      bool more()
      {
         return nents > curEnt;
      }

      bool next(uint64_t& addr, uint64_t& len, uint64_t& offset)
      {
         if (!more())
            return false;
         addr = dma_address[curEnt];
         len = dma_length[curEnt];
         offset = dma_offset[curEnt];
         curEnt++;
         return true;
      }

};
#endif /* BEEGFS_NVFS */

#endif /* _RDMAINFO_H_ */

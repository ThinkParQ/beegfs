#ifndef CHUNKBLOCKSVEC_H_
#define CHUNKBLOCKSVEC_H_

#include <common/toolkit/serialization/Serialization.h>
#include <common/app/log/LogContext.h>

#include <numeric>

class ChunksBlocksVec
{
   public:

      ChunksBlocksVec()
      {
      }

      ChunksBlocksVec(size_t numTargets)
         : chunkBlocksVec(numTargets, 0)
      {
      }

   private:
      UInt64Vector chunkBlocksVec; // number of blocks for each chunk file

   public:
      // setters

      /**
       * Set the number of used blocks for the chunk-file of this target
       */
      void setNumBlocks(size_t target, uint64_t numBlocks, size_t numStripeTargets)
      {
         const char* logContext = "ChunksBlocksVec set num blocks";

         if (this->chunkBlocksVec.empty() )
            this->chunkBlocksVec.resize(numStripeTargets, 0);

         if (unlikely(target >= this->chunkBlocksVec.size() ) )
         {
            LogContext(logContext).logErr("Bug: target > vec-size");
            LogContext(logContext).logBacktrace();
            return;
         }

         this->chunkBlocksVec.at(target) = numBlocks;
      }

      // getters

      /**
       * Get the number of blocks for the given target
       */
      uint64_t getNumBlocks(size_t target) const
      {
         const char* logContext = "ChunksBlocksVec get number of blocks";

         if (this->chunkBlocksVec.empty() )
            return 0;

         if (unlikely(target >= this->chunkBlocksVec.size() ) )
         {
            LogContext(logContext).logErr("Bug: target number larger than stripes");
            LogContext(logContext).logBacktrace();
            return 0;
         }

         return this->chunkBlocksVec.at(target);
      }

      /**
       * Get the sum of all used blocks of all targets
       */
      uint64_t getBlockSum() const
      {
         return std::accumulate(this->chunkBlocksVec.begin(), this->chunkBlocksVec.end(), 0ULL);
      }

      // inlined
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->chunkBlocksVec;
      }

      bool operator==(const ChunksBlocksVec& other) const
      {
         return chunkBlocksVec == other.chunkBlocksVec;
      }

      bool operator!=(const ChunksBlocksVec& other) const { return !(*this == other); }
};


#endif /* CHUNKBLOCKSVEC_H_ */

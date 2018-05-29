#ifndef RETRIEVEINODESMSG_H
#define RETRIEVEINODESMSG_H

#include <common/net/message/NetMessage.h>

class RetrieveInodesMsg : public NetMessageSerdes<RetrieveInodesMsg>
{
   public:
      RetrieveInodesMsg(unsigned hashDirNum, int64_t lastOffset, unsigned maxOutInodes,
            bool isBuddyMirrored):
         BaseType(NETMSGTYPE_RetrieveInodes),
         hashDirNum(hashDirNum), lastOffset(lastOffset), maxOutInodes(maxOutInodes),
         isBuddyMirrored(isBuddyMirrored)
      {
      }

      RetrieveInodesMsg() : BaseType(NETMSGTYPE_RetrieveInodes)
      {
      }

   private:
      uint32_t hashDirNum;
      int64_t lastOffset;
      uint32_t maxOutInodes;
      bool isBuddyMirrored;

   public:
      uint getHashDirNum()
      {
         return hashDirNum;
      }

      int64_t getLastOffset()
      {
         return lastOffset;
      }

      uint getMaxOutInodes()
      {
         return maxOutInodes;
      }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->hashDirNum
            % obj->lastOffset
            % obj->maxOutInodes
            % obj->isBuddyMirrored;
      }
};


#endif /*RETRIEVEINODESMSG_H*/

#ifndef ADJUSTCHUNKPERMISSIONSMSG_H
#define ADJUSTCHUNKPERMISSIONSMSG_H

#include <common/net/message/NetMessage.h>

class AdjustChunkPermissionsMsg : public NetMessageSerdes<AdjustChunkPermissionsMsg>
{
   public:
      AdjustChunkPermissionsMsg(unsigned hashDirNum, std::string& currentContDirID,
         unsigned maxEntries, int64_t lastHashDirOffset, int64_t lastContDirOffset,
         bool isBuddyMirrored) :
         BaseType(NETMSGTYPE_AdjustChunkPermissions)
      {
         this->hashDirNum = hashDirNum;
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->maxEntries = maxEntries;
         this->lastHashDirOffset = lastHashDirOffset;
         this->lastContDirOffset = lastContDirOffset;
         this->isBuddyMirrored = isBuddyMirrored;
      }

      AdjustChunkPermissionsMsg() : BaseType(NETMSGTYPE_AdjustChunkPermissions)
      {
      }

   private:
      uint32_t hashDirNum;
      const char *currentContDirID;
      uint32_t currentContDirIDLen;
      uint32_t maxEntries;
      int64_t lastHashDirOffset;
      int64_t lastContDirOffset;
      bool isBuddyMirrored;

   public:
      std::string getCurrentContDirID() const
      {
          return currentContDirID;
      }

      unsigned getHashDirNum() const
      {
          return hashDirNum;
      }

      int64_t getLastContDirOffset() const
      {
          return lastContDirOffset;
      }

      int64_t getLastHashDirOffset() const
      {
          return lastHashDirOffset;
      }

      unsigned getMaxEntries() const
      {
          return maxEntries;
      }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->currentContDirID, obj->currentContDirIDLen, 4)
            % obj->hashDirNum
            % obj->maxEntries
            % obj->lastHashDirOffset
            % obj->lastContDirOffset
            % obj->isBuddyMirrored;
      }
};

#endif /*ADJUSTCHUNKPERMISSIONSMSG_H*/

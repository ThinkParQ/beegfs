#ifndef RETRIEVEDIRENTRIESMSG_H
#define RETRIEVEDIRENTRIESMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>

class RetrieveDirEntriesMsg : public NetMessageSerdes<RetrieveDirEntriesMsg>
{
   public:
      RetrieveDirEntriesMsg(unsigned hashDirNum, std::string& currentContDirID,
         unsigned maxOutEntries, int64_t lastHashDirOffset, int64_t lastContDirOffset,
         bool isBuddyMirrored) :
         BaseType(NETMSGTYPE_RetrieveDirEntries)
      {
         this->hashDirNum = hashDirNum;
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->maxOutEntries = maxOutEntries;
         this->lastHashDirOffset = lastHashDirOffset;
         this->lastContDirOffset = lastContDirOffset;
         this->isBuddyMirrored = isBuddyMirrored;
      }

      RetrieveDirEntriesMsg() : BaseType(NETMSGTYPE_RetrieveDirEntries)
      {
      }

private:
    uint32_t hashDirNum;
    const char *currentContDirID;
    unsigned currentContDirIDLen;
    uint32_t maxOutEntries;
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

    unsigned getMaxOutEntries() const
    {
        return maxOutEntries;
    }

    bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->hashDirNum
            % serdes::rawString(obj->currentContDirID, obj->currentContDirIDLen)
            % obj->maxOutEntries
            % obj->lastHashDirOffset
            % obj->lastContDirOffset
            % obj->isBuddyMirrored;
      }
};

#endif /*RETRIEVEDIRENTRIESMSG_H*/

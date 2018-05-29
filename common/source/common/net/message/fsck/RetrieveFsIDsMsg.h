#ifndef RETRIEVEFSIDSMSG_H
#define RETRIEVEFSIDSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>

class RetrieveFsIDsMsg : public NetMessageSerdes<RetrieveFsIDsMsg>
{
   public:
      RetrieveFsIDsMsg(unsigned hashDirNum, bool buddyMirrored, std::string& currentContDirID,
         unsigned maxOutIDs, int64_t lastHashDirOffset, int64_t lastContDirOffset) :
         BaseType(NETMSGTYPE_RetrieveFsIDs)
      {
         this->hashDirNum = hashDirNum;
         this->buddyMirrored = buddyMirrored;
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->maxOutIDs = maxOutIDs;
         this->lastHashDirOffset = lastHashDirOffset;
         this->lastContDirOffset = lastContDirOffset;
      }

      RetrieveFsIDsMsg() : BaseType(NETMSGTYPE_RetrieveFsIDs)
      {
      }

private:
    uint32_t hashDirNum;
    bool buddyMirrored;
    const char *currentContDirID;
    unsigned currentContDirIDLen;
    uint32_t maxOutIDs;
    int64_t lastHashDirOffset;
    int64_t lastContDirOffset;

public:
    std::string getCurrentContDirID() const
    {
        return currentContDirID;
    }

    unsigned getHashDirNum() const
    {
        return hashDirNum;
    }

    bool getBuddyMirrored() const
    {
       return buddyMirrored;
    }

    int64_t getLastContDirOffset() const
    {
        return lastContDirOffset;
    }

    int64_t getLastHashDirOffset() const
    {
        return lastHashDirOffset;
    }

    unsigned getMaxOutIDs() const
    {
        return maxOutIDs;
    }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->hashDirNum
            % obj->buddyMirrored
            % serdes::rawString(obj->currentContDirID, obj->currentContDirIDLen)
            % obj->maxOutIDs
            % obj->lastHashDirOffset
            % obj->lastContDirOffset;
      }
};

#endif /*RETRIEVEFSIDSMSG_H*/

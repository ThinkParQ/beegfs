#ifndef RETRIEVEFSIDSMSG_H
#define RETRIEVEFSIDSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>

class RetrieveFsIDsMsg : public NetMessage
{
   public:
      RetrieveFsIDsMsg(unsigned hashDirNum, std::string& currentContDirID,
         unsigned maxOutIDs, int64_t lastHashDirOffset, int64_t lastContDirOffset) :
         NetMessage(NETMSGTYPE_RetrieveFsIDs)
      {
         this->hashDirNum = hashDirNum;
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->maxOutIDs = maxOutIDs;
         this->lastHashDirOffset = lastHashDirOffset;
         this->lastContDirOffset = lastContDirOffset;
      }

   protected:
      RetrieveFsIDsMsg() : NetMessage(NETMSGTYPE_RetrieveFsIDs)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt() + // hashDirNum
            Serialization::serialLenStrAlign4(currentContDirIDLen) + // currentContDirID
            Serialization::serialLenUInt() + // maxOutIDs
            Serialization::serialLenInt64() + //lastHashDirOffset
            Serialization::serialLenInt64(); // lastContDirOffset
    }

private:
    unsigned hashDirNum;
    const char *currentContDirID;
    unsigned currentContDirIDLen;
    unsigned maxOutIDs;
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
      
};

#endif /*RETRIEVEFSIDSMSG_H*/

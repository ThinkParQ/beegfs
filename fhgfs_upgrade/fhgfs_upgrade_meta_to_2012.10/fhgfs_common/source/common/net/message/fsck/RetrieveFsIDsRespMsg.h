#ifndef RETRIEVEFSIDSRESPMSG_H
#define RETRIEVEFSIDSRESPMSG_H

#include <common/Common.h>
#include <common/fsck/FsckFsID.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RetrieveFsIDsRespMsg : public NetMessage
{
   public:
      RetrieveFsIDsRespMsg(FsckFsIDList* fsckFsIDs, std::string& currentContDirID,
         int64_t newHashDirOffset, int64_t newContDirOffset) :
            NetMessage(NETMSGTYPE_RetrieveFsIDsResp)
      {
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->newHashDirOffset = newHashDirOffset;
         this->newContDirOffset = newContDirOffset;
         this->fsckFsIDs = fsckFsIDs;
      }

      RetrieveFsIDsRespMsg() : NetMessage(NETMSGTYPE_RetrieveFsIDsResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStrAlign4(currentContDirIDLen) + // currentContDirID
            Serialization::serialLenInt64() + // newHashDirOffset
            Serialization::serialLenInt64() + // newContDirOffset
            SerializationFsck::serialLenFsckFsIDList(fsckFsIDs); // fsckFsIDs
      }
      
   private:
      const char* currentContDirID;
      unsigned currentContDirIDLen;
      int64_t newHashDirOffset;
      int64_t newContDirOffset;

      FsckFsIDList* fsckFsIDs;
      
      // for deserialization
      unsigned fsckFsIDsElemNum;
      const char* fsckFsIDsStart;
      unsigned fsckFsIDsBufLen;

   public:
      // inliners   
      void parseFsIDs(FsckFsIDList* outList)
      {
         SerializationFsck::deserializeFsckFsIDList(fsckFsIDsElemNum, fsckFsIDsStart,
            outList);
      }

      // getters & setters
      int64_t getNewHashDirOffset() const
      {
         return newHashDirOffset;
      }

      int64_t getNewContDirOffset() const
      {
         return newContDirOffset;
      }

      std::string getCurrentContDirID() const
      {
         return currentContDirID;
      }
};

#endif /*RETRIEVEFSIDSRESPMSG_H*/

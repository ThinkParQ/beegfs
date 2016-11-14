#ifndef CREATEDEFDIRINODESRESPMSG_H
#define CREATEDEFDIRINODESRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class CreateDefDirInodesRespMsg : public NetMessage
{
   public:
      CreateDefDirInodesRespMsg(StringList* failedInodeIDs, FsckDirInodeList* createdInodes) :
         NetMessage(NETMSGTYPE_CreateDefDirInodesResp)
      {
         this->failedInodeIDs = failedInodeIDs;
         this->createdInodes = createdInodes;
      }

      virtual ~CreateDefDirInodesRespMsg() { };

      CreateDefDirInodesRespMsg() : NetMessage(NETMSGTYPE_CreateDefDirInodesResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStringList(this->failedInodeIDs) + // inodeIDs
            SerializationFsck::serialLenFsckDirInodeList(this->createdInodes); // createdInodes
      }

   private:
      StringList* failedInodeIDs;
      const char* failedInodeIDsStart;
      unsigned failedInodeIDsBufLen;
      unsigned failedInodeIDsElemNum;

      FsckDirInodeList* createdInodes;
      const char* createdInodesStart;
      unsigned createdInodesElemNum;
      unsigned createdInodesBufLen;

   public:
      void parseFailedInodeIDs(StringList* outInodeIDs)
      {
         Serialization::deserializeStringList(failedInodeIDsBufLen, failedInodeIDsElemNum,
            failedInodeIDsStart, outInodeIDs );
      }

      void parseCreatedInodes(FsckDirInodeList* outInodes)
      {
         SerializationFsck::deserializeFsckDirInodeList(createdInodesElemNum, createdInodesStart,
            outInodes );
      }
};

#endif /* CREATEDEFDIRINODESRESPMSG_H */

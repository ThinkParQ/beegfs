#ifndef UPDATEDIRATTRIBSRESPMSG_H
#define UPDATEDIRATTRIBSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class UpdateDirAttribsRespMsg : public NetMessage
{
   public:
      UpdateDirAttribsRespMsg(FsckDirInodeList* failedInodes) :
         NetMessage(NETMSGTYPE_UpdateDirAttribsResp)
      {
         this->failedInodes = failedInodes;
      }

      UpdateDirAttribsRespMsg() : NetMessage(NETMSGTYPE_UpdateDirAttribsResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckDirInodeList(this->failedInodes); // failedInodes
      }

   private:
      FsckDirInodeList* failedInodes;
      unsigned failedInodesBufLen;
      unsigned failedInodesElemNum;
      const char* failedInodesStart;

   public:
      void parseFailedInodes(FsckDirInodeList* outFailedInodes)
      {
         SerializationFsck::deserializeFsckDirInodeList(failedInodesElemNum, failedInodesStart,
            outFailedInodes);
      }
};


#endif /*UPDATEDIRATTRIBSRESPMSG_H*/

#ifndef UPDATEFILEATTRIBSRESPMSG_H
#define UPDATEFILEATTRIBSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class UpdateFileAttribsRespMsg : public NetMessage
{
   public:
      UpdateFileAttribsRespMsg(FsckFileInodeList* failedInodes) :
         NetMessage(NETMSGTYPE_UpdateFileAttribsResp)
      {
         this->failedInodes = failedInodes;
      }

      UpdateFileAttribsRespMsg() : NetMessage(NETMSGTYPE_UpdateFileAttribsResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckFileInodeList(this->failedInodes); // failedInodes
      }

   private:
      FsckFileInodeList* failedInodes;
      unsigned failedInodesBufLen;
      unsigned failedInodesElemNum;
      const char* failedInodesStart;

   public:
      void parseFailedInodes(FsckFileInodeList* outFailedInodes)
      {
         SerializationFsck::deserializeFsckFileInodeList(failedInodesElemNum, failedInodesStart,
            outFailedInodes);
      }
};


#endif /*UPDATEFILEATTRIBSRESPMSG_H*/

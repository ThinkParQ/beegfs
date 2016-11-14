#ifndef FIXINODEOWNERSRESPMSG_H
#define FIXINODEOWNERSRESPMSG_H

#include <common/toolkit/serialization/SerializationFsck.h>
#include <common/net/message/NetMessage.h>

class FixInodeOwnersRespMsg : public NetMessage
{
   public:
      FixInodeOwnersRespMsg(FsckDirInodeList* failedInodes) :
         NetMessage(NETMSGTYPE_FixInodeOwnersResp)
      {
         this->failedInodes = failedInodes;
      }

      FixInodeOwnersRespMsg() : NetMessage(NETMSGTYPE_FixInodeOwnersResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckDirInodeList(this->failedInodes); // failedEntries
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


#endif /*FIXINODEOWNERSRESPMSG_H*/

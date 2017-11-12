#ifndef RECREATEFSIDSRESPMSG_H
#define RECREATEFSIDSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RecreateFsIDsRespMsg : public NetMessage
{
   public:
      RecreateFsIDsRespMsg(FsckDirEntryList* failedEntries) :
         NetMessage(NETMSGTYPE_RecreateFsIDsResp)
      {
         this->failedEntries = failedEntries;
      }

      RecreateFsIDsRespMsg() : NetMessage(NETMSGTYPE_RecreateFsIDsResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckDirEntryList(this->failedEntries); // failedEntries
      }

   private:
      FsckDirEntryList* failedEntries;
      unsigned failedEntriesBufLen;
      unsigned failedEntriesElemNum;
      const char* failedEntriesStart;

   public:
      void parseFailedEntries(FsckDirEntryList* outFailedEntries)
      {
         SerializationFsck::deserializeFsckDirEntryList(failedEntriesElemNum, failedEntriesStart,
            outFailedEntries);
      }
};


#endif /*RECREATEFSIDSRESPMSG_H*/

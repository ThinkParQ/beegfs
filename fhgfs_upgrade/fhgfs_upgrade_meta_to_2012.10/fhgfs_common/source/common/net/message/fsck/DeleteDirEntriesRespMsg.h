#ifndef DELETEDIRENTRIESRESPMSG_H
#define DELETEDIRENTRIESRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class DeleteDirEntriesRespMsg : public NetMessage
{
   public:
      DeleteDirEntriesRespMsg(FsckDirEntryList* failedEntries) :
         NetMessage(NETMSGTYPE_DeleteDirEntriesResp)
      {
         this->failedEntries = failedEntries;
      }

      DeleteDirEntriesRespMsg() : NetMessage(NETMSGTYPE_DeleteDirEntriesResp)
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


#endif /*DELETEDIRENTRIESRESPMSG_H*/

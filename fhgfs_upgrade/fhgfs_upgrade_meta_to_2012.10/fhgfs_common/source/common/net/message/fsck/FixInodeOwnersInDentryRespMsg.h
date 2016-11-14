#ifndef FIXINODEOWNERSINDENTRYRESPMSG_H
#define FIXINODEOWNERSINDENTRYRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class FixInodeOwnersInDentryRespMsg : public NetMessage
{
   public:
      FixInodeOwnersInDentryRespMsg(FsckDirEntryList* failedEntries) :
         NetMessage(NETMSGTYPE_FixInodeOwnersInDentryResp)
      {
         this->failedEntries = failedEntries;
      }

      FixInodeOwnersInDentryRespMsg() : NetMessage(NETMSGTYPE_FixInodeOwnersInDentryResp)
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


#endif /*FIXINODEOWNERSINDENTRYRESPMSG_H*/

#ifndef DELETEDIRENTRIESMSG_H
#define DELETEDIRENTRIESMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class DeleteDirEntriesMsg : public NetMessage
{
   public:
      DeleteDirEntriesMsg(FsckDirEntryList* entries) : NetMessage(NETMSGTYPE_DeleteDirEntries)
      {
         this->entries = entries;
      }


   protected:
      DeleteDirEntriesMsg() : NetMessage(NETMSGTYPE_DeleteDirEntries)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckDirEntryList(this->entries); // entries
      }


   private:
      FsckDirEntryList* entries;
      unsigned entriesBufLen;
      unsigned entriesElemNum;
      const char* entriesStart;

   public:
      void parseEntries(FsckDirEntryList* outEntries)
      {
         SerializationFsck::deserializeFsckDirEntryList(entriesElemNum, entriesStart, outEntries);
      }
};


#endif /*DELETEDIRENTRIESMSG_H*/

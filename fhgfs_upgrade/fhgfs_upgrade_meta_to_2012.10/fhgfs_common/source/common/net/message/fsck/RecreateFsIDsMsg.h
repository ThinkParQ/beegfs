#ifndef RECREATEFSIDSMSG_H
#define RECREATEFSIDSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RecreateFsIDsMsg : public NetMessage
{
   public:
      RecreateFsIDsMsg(FsckDirEntryList* entries) : NetMessage(NETMSGTYPE_RecreateFsIDs)
      {
         this->entries = entries;
      }


   protected:
      RecreateFsIDsMsg() : NetMessage(NETMSGTYPE_RecreateFsIDs)
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


#endif /*RECREATEFSIDSMSG_H*/

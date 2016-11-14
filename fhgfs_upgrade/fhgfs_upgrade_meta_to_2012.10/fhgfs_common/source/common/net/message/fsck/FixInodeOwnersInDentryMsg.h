#ifndef FIXINODEOWNERSINDENTRYMSG_H
#define FIXINODEOWNERSINDENTRYMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class FixInodeOwnersInDentryMsg : public NetMessage
{
   public:
      FixInodeOwnersInDentryMsg(FsckDirEntryList* dentries) :
         NetMessage(NETMSGTYPE_FixInodeOwnersInDentry)
      {
         this->dentries = dentries;
      }

   protected:
      FixInodeOwnersInDentryMsg(): NetMessage(NETMSGTYPE_FixInodeOwnersInDentry)
      {
      }

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckDirEntryList(this->dentries); // dentries
      }

   private:
      FsckDirEntryList* dentries;
      unsigned dentriesBufLen;
      unsigned dentriesElemNum;
      const char* dentriesStart;

   public:
      void parseDentries(FsckDirEntryList* outDentries)
      {
         SerializationFsck::deserializeFsckDirEntryList(dentriesElemNum, dentriesStart,
            outDentries);
      }
};


#endif /*FIXINODEOWNERSINDENTRYMSG_H*/

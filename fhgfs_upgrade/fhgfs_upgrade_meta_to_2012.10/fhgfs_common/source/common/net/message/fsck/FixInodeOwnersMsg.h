#ifndef FIXINODEOWNERSMSG_H
#define FIXINODEOWNERSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class FixInodeOwnersMsg : public NetMessage
{
   public:
      FixInodeOwnersMsg(FsckDirInodeList* inodes) : NetMessage(NETMSGTYPE_FixInodeOwners)
      {
         this->inodes = inodes;
      }

   protected:
      FixInodeOwnersMsg(): NetMessage(NETMSGTYPE_FixInodeOwners)
      {
      }

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckDirInodeList(this->inodes); // inodes
      }

   private:
      FsckDirInodeList* inodes;

      // for deserialization
      unsigned inodesBufLen;
      unsigned inodesElemNum;
      const char* inodesStart;

   public:
      void parseInodes(FsckDirInodeList* outInodes)
      {
         SerializationFsck::deserializeFsckDirInodeList(inodesElemNum, inodesStart, outInodes);
      }
};


#endif /*FIXINODEOWNERSMSG_H*/

#ifndef UPDATEDIRATTRIBSMSG_H
#define UPDATEDIRATTRIBSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class UpdateDirAttribsMsg : public NetMessage
{
   public:
      UpdateDirAttribsMsg(FsckDirInodeList* inodes) :
         NetMessage(NETMSGTYPE_UpdateDirAttribs)
      {
         this->inodes = inodes;
      }

   protected:
      UpdateDirAttribsMsg() : NetMessage(NETMSGTYPE_UpdateDirAttribs)
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
      unsigned inodesBufLen;
      unsigned inodesElemNum;
      const char* inodesStart;

   public:
      void parseInodes(FsckDirInodeList* outInodes)
      {
         SerializationFsck::deserializeFsckDirInodeList(inodesElemNum, inodesStart,
            outInodes);
      }
};


#endif /*UPDATEDIRATTRIBSMSG_H*/

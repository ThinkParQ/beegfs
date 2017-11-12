#ifndef UPDATEFILEATTRIBSMSG_H
#define UPDATEFILEATTRIBSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class UpdateFileAttribsMsg : public NetMessage
{
   public:
      UpdateFileAttribsMsg(FsckFileInodeList* inodes) :
         NetMessage(NETMSGTYPE_UpdateFileAttribs)
      {
         this->inodes = inodes;
      }

   protected:
      UpdateFileAttribsMsg() : NetMessage(NETMSGTYPE_UpdateFileAttribs)
      {
      }

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckFileInodeList(this->inodes); // inodes
      }

   private:
      FsckFileInodeList* inodes;
      unsigned inodesBufLen;
      unsigned inodesElemNum;
      const char* inodesStart;

   public:
      void parseInodes(FsckFileInodeList* outInodes)
      {
         SerializationFsck::deserializeFsckFileInodeList(inodesElemNum, inodesStart,
            outInodes);
      }
};


#endif /*UPDATEFILEATTRIBSMSG_H*/

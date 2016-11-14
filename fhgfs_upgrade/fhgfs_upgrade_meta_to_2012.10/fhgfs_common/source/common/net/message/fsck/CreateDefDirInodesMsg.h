#ifndef CREATEDEFDIRINODESMSG_H
#define CREATEDEFDIRINODESMSG_H

#include <common/net/message/NetMessage.h>

class CreateDefDirInodesMsg : public NetMessage
{
   public:
      CreateDefDirInodesMsg(StringList* inodeIDs) : NetMessage(NETMSGTYPE_CreateDefDirInodes)
      {
         this->inodeIDs = inodeIDs;
      }

      virtual ~CreateDefDirInodesMsg() { };

   protected:
      CreateDefDirInodesMsg() : NetMessage(NETMSGTYPE_CreateDefDirInodes)
      {
      }

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStringList(this->inodeIDs); // inodeIDs
      }

   private:
      StringList* inodeIDs;
      const char* inodeIDsStart;
      unsigned inodeIDsBufLen;
      unsigned inodeIDsElemNum;

   public:
      void parseInodeIDs(StringList* outInodeIDs)
      {
         Serialization::deserializeStringList(inodeIDsBufLen, inodeIDsElemNum, inodeIDsStart,
            outInodeIDs );
      }
};

#endif /* CREATEDEFDIRINODESMSG_H */

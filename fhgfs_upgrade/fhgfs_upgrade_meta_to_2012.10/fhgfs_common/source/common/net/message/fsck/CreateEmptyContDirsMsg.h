#ifndef CREATEEMPTYCONTDIRSMSG_H_
#define CREATEEMPTYCONTDIRSMSG_H_

#include <common/net/message/NetMessage.h>

class CreateEmptyContDirsMsg : public NetMessage
{
   public:
      CreateEmptyContDirsMsg(StringList* dirIDs) : NetMessage(NETMSGTYPE_CreateEmptyContDirs)
      {
         this->dirIDs = dirIDs;
      }

   protected:
      CreateEmptyContDirsMsg() : NetMessage(NETMSGTYPE_CreateEmptyContDirs)
      {
      }

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStringList(this->dirIDs); // dirIDs
      }

   private:
      StringList* dirIDs;

      // for deserialization
      unsigned dirIDsElemNum;
      unsigned dirIDsBufLen;
      const char* dirIDsStart;

   public:
      void parseDirIDs(StringList* outDirIDs)
      {
         Serialization::deserializeStringList(dirIDsBufLen, dirIDsElemNum,
            dirIDsStart, outDirIDs);
      }
};

#endif /* CREATEEMPTYCONTDIRSMSG_H_ */

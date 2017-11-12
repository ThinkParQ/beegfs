#ifndef CREATEEMPTYCONTDIRSRESPMSG_H
#define CREATEEMPTYCONTDIRSRESPMSG_H

#include <common/net/message/NetMessage.h>

class CreateEmptyContDirsRespMsg : public NetMessage
{
   public:
      CreateEmptyContDirsRespMsg(StringList* failedDirIDs) : NetMessage(NETMSGTYPE_CreateEmptyContDirsResp)
      {
         this->failedDirIDs = failedDirIDs;
      }

      CreateEmptyContDirsRespMsg() : NetMessage(NETMSGTYPE_CreateEmptyContDirsResp)
      {
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStringList(this->failedDirIDs); // failedDirIDs
      }

   private:
      StringList* failedDirIDs;

      // for deserialization
      unsigned failedDirIDsElemNum;
      unsigned failedDirIDsBufLen;
      const char* failedDirIDsStart;

   public:
      void parseFailedIDs(StringList* outFailedIDs)
      {
         Serialization::deserializeStringList(failedDirIDsBufLen, failedDirIDsElemNum,
            failedDirIDsStart, outFailedIDs);
      }
};

#endif /* CREATEEMPTYCONTDIRSRESPMSG_H */

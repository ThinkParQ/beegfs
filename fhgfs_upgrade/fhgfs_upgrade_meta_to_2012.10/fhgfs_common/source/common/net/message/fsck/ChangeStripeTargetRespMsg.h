#ifndef CHANGESTRIPETARGETRESPMSG_H
#define CHANGESTRIPETARGETRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class ChangeStripeTargetRespMsg: public NetMessage
{
   public:
      /*
       * @param failedInodes a list of file inodes, which failed to be updated
       */
      ChangeStripeTargetRespMsg(FsckFileInodeList* failedInodes) :
         NetMessage(NETMSGTYPE_ChangeStripeTargetResp)
      {
         this->failedInodes = failedInodes;
      }

      // only for deserialization
      ChangeStripeTargetRespMsg() : NetMessage(NETMSGTYPE_ChangeStripeTargetResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckObjectList(failedInodes); // failedEntryIDs
      }

   private:
      FsckFileInodeList* failedInodes;

      // for deserialization
      const char* failedInodesStart;
      unsigned failedInodesBufLen;
      unsigned failedInodesElemNum;

   public:
      // getter
      void parseFailedInodes(FsckFileInodeList* outFailedInodes)
      {
         SerializationFsck::deserializeFsckObjectList(failedInodesElemNum,
            failedInodesStart, outFailedInodes);
      }

      // inliner
      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         ChangeStripeTargetRespMsg* msgIn = (ChangeStripeTargetRespMsg*) msg;
         FsckFileInodeList failedInodesIn;
         msgIn->parseFailedInodes(&failedInodesIn);

         if (! FsckTk::listsEqual(this->failedInodes, &failedInodesIn) )
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }
};

#endif /*CHANGESTRIPETARGETRESPMSG_H*/

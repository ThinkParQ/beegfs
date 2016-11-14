#ifndef CHANGESTRIPETARGETMSG_H
#define CHANGESTRIPETARGETMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>
#include <common/toolkit/FsckTk.h>

class ChangeStripeTargetMsg: public NetMessage
{
   public:
      ChangeStripeTargetMsg(FsckFileInodeList* inodes, uint16_t targetID, uint16_t newTargetID) :
         NetMessage(NETMSGTYPE_ChangeStripeTarget)
      {
         this->inodes = inodes;
         this->targetID = targetID;
         this->newTargetID = newTargetID;
      }

      // only for deserialization
      ChangeStripeTargetMsg() : NetMessage(NETMSGTYPE_ChangeStripeTarget)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckObjectList(inodes) + // inodes
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenUShort(); // newTargetID
      }

   private:
      FsckFileInodeList* inodes; // only files listed here will get corrected!
      uint16_t targetID;
      uint16_t newTargetID;

      // for deserialization
      const char* inodesStart;
      unsigned inodesBufLen;
      unsigned inodesElemNum;

   public:
      // getter
      void parseInodes(FsckFileInodeList* outInodes)
      {
         SerializationFsck::deserializeFsckObjectList(inodesElemNum, inodesStart, outInodes);
      }

      uint16_t getTargetID()
      {
         return this->targetID;
      }

      uint16_t getNewTargetID()
      {
         return this->newTargetID;
      }

      // inliner
      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         ChangeStripeTargetMsg* msgIn = (ChangeStripeTargetMsg*) msg;
         FsckFileInodeList inodesIn;
         msgIn->parseInodes(&inodesIn);

         if ( ! FsckTk::listsEqual(this->inodes, &inodesIn) )
            return TestingEqualsRes_FALSE;

         if ( this->getTargetID() != msgIn->getTargetID() )
            return TestingEqualsRes_FALSE;

         if ( this->getNewTargetID() != msgIn->getNewTargetID() )
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }
};

#endif /*CHANGESTRIPETARGETMSG_H*/

#ifndef RECREATEDENTRIESMSG_H
#define RECREATEDENTRIESMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RecreateDentriesMsg : public NetMessage
{
   public:
      RecreateDentriesMsg(FsckFsIDList* fsIDs) : NetMessage(NETMSGTYPE_RecreateDentries)
      {
         this->fsIDs = fsIDs;
      }


   protected:
      RecreateDentriesMsg() : NetMessage(NETMSGTYPE_RecreateDentries)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckObjectList(this->fsIDs); // fsIDs
      }


   private:
      FsckFsIDList* fsIDs;
      unsigned fsIDsElemNum;
      const char* fsIDsStart;

   public:
      void parseFsIDs(FsckFsIDList* outFsIDs)
      {
         SerializationFsck::deserializeFsckObjectList(fsIDsElemNum, fsIDsStart, outFsIDs);
      }
};


#endif /*RECREATEDENTRIESMSG_H*/

#ifndef RECREATEDENTRIESRESPMSG_H
#define RECREATEDENTRIESRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RecreateDentriesRespMsg : public NetMessage
{
   public:
      RecreateDentriesRespMsg(FsckFsIDList* failedCreates, FsckDirEntryList* createdDentries,
         FsckFileInodeList* createdInodes) :
         NetMessage(NETMSGTYPE_RecreateDentriesResp)
      {
         this->failedCreates = failedCreates;
         this->createdDentries = createdDentries;
         this->createdInodes = createdInodes;
      }

      RecreateDentriesRespMsg() : NetMessage(NETMSGTYPE_RecreateDentriesResp)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckObjectList(this->failedCreates) + // failedCreates
            SerializationFsck::serialLenFsckObjectList(this->createdDentries) + // createdDentries
            SerializationFsck::serialLenFsckObjectList(this->createdInodes); // createdInodes
      }


   private:
      FsckFsIDList* failedCreates;
      unsigned failedCreatesElemNum;
      const char* failedCreatesStart;

      FsckDirEntryList* createdDentries;
      unsigned createdDentriesElemNum;
      const char* createdDentriesStart;

      FsckFileInodeList* createdInodes;
      unsigned createdInodesElemNum;
      const char* createdInodesStart;

   public:
      void parseFailedCreates(FsckFsIDList* outFailedCreates)
      {
         SerializationFsck::deserializeFsckObjectList(failedCreatesElemNum, failedCreatesStart,
            outFailedCreates);
      }

      void parseCreatedDentries(FsckDirEntryList* createdDentries)
      {
         SerializationFsck::deserializeFsckObjectList(createdDentriesElemNum, createdDentriesStart,
            createdDentries);
      }

      void parseCreatedInodes(FsckFileInodeList* createdInodes)
      {
         SerializationFsck::deserializeFsckObjectList(createdInodesElemNum, createdInodesStart,
            createdInodes);
      }
};


#endif /*RECREATEDENTRIESRESPMSG_H*/

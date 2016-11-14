#ifndef RETRIEVEINODESRESPMSG_H
#define RETRIEVEINODESRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RetrieveInodesRespMsg : public NetMessage
{
   public:
      RetrieveInodesRespMsg(FsckFileInodeList *fileInodes, FsckDirInodeList *dirInodes,
         int64_t lastOffset)  : NetMessage(NETMSGTYPE_RetrieveInodesResp)
      {
         this->fileInodes = fileInodes;
         this->dirInodes = dirInodes;
         this->lastOffset = lastOffset;

      }

      RetrieveInodesRespMsg() : NetMessage(NETMSGTYPE_RetrieveInodesResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            SerializationFsck::serialLenFsckFileInodeList(fileInodes) +
            SerializationFsck::serialLenFsckDirInodeList(dirInodes) +
            Serialization::serialLenInt64();
      }


   private:
      FsckFileInodeList* fileInodes;
      FsckDirInodeList* dirInodes;
      int64_t lastOffset;

      // for deserialization
      unsigned fsckFileInodeListElemNum;
      unsigned fsckFileInodeListBufLen;
      const char*fsckFileInodeListStart;

      unsigned fsckDirInodeListElemNum;
      unsigned fsckDirInodeListBufLen;
      const char*fsckDirInodeListStart;

   public:
      void parseFileInodes(FsckFileInodeList* outFileInodes)
      {
         SerializationFsck::deserializeFsckFileInodeList(fsckFileInodeListElemNum,
            fsckFileInodeListStart, outFileInodes);
      }

      void parseDirInodes(FsckDirInodeList* outDirInodes)
      {
         SerializationFsck::deserializeFsckDirInodeList(fsckDirInodeListElemNum,
            fsckDirInodeListStart, outDirInodes);
      }

      int64_t getLastOffset()
      {
         return lastOffset;
      }
};


#endif /*RETRIEVEINODESRESPMSG_H*/

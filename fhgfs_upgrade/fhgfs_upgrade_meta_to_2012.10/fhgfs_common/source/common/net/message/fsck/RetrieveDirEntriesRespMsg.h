#ifndef RETRIEVEDIRENTRIESRESPMSG_H
#define RETRIEVEDIRENTRIESRESPMSG_H

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class RetrieveDirEntriesRespMsg : public NetMessage
{
   public:
      RetrieveDirEntriesRespMsg(FsckContDirList* fsckContDirs, FsckDirEntryList* fsckDirEntries,
         FsckFileInodeList* inlinedFileInodes, std::string& currentContDirID,
         int64_t newHashDirOffset, int64_t newContDirOffset) :
            NetMessage(NETMSGTYPE_RetrieveDirEntriesResp)
      {
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->newHashDirOffset = newHashDirOffset;
         this->newContDirOffset = newContDirOffset;
         this->fsckContDirs = fsckContDirs;
         this->fsckDirEntries = fsckDirEntries;
         this->inlinedFileInodes = inlinedFileInodes;
      }

      RetrieveDirEntriesRespMsg() : NetMessage(NETMSGTYPE_RetrieveDirEntriesResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStrAlign4(currentContDirIDLen) + // currentContDirID
            Serialization::serialLenInt64() + // newHashDirOffset
            Serialization::serialLenInt64() + // newContDirOffset
            SerializationFsck::serialLenFsckContDirList(fsckContDirs) + // fsckContDirList
            SerializationFsck::serialLenFsckDirEntryList(fsckDirEntries) + // fsckDirEntries
            SerializationFsck::serialLenFsckFileInodeList(inlinedFileInodes); // inlinedFileInodes
      }
      
   private:
      const char* currentContDirID;
      unsigned currentContDirIDLen;
      int64_t newHashDirOffset;
      int64_t newContDirOffset;

      FsckContDirList* fsckContDirs;
      FsckDirEntryList* fsckDirEntries;
      FsckFileInodeList* inlinedFileInodes;
      
      // for deserialization
      unsigned fsckContDirsElemNum;
      const char* fsckContDirsStart;
      unsigned fsckContDirsBufLen;

      unsigned fsckDirEntriesElemNum;
      const char* fsckDirEntriesStart;
      unsigned fsckDirEntriesBufLen;
      
      unsigned inlinedFileInodesElemNum;
      const char* inlinedFileInodesStart;
      unsigned inlinedFileInodesBufLen;

   public:
      // inliners   
      void parseContDirs(FsckContDirList* outList)
      {
         SerializationFsck::deserializeFsckContDirList(fsckContDirsElemNum, fsckContDirsStart,
            outList);
      }

      void parseDirEntries(FsckDirEntryList* outList)
      {
         SerializationFsck::deserializeFsckDirEntryList(fsckDirEntriesElemNum, fsckDirEntriesStart,
            outList);
      }

      void parseInlinedFileInodes(FsckFileInodeList* outList)
      {
         SerializationFsck::deserializeFsckFileInodeList(inlinedFileInodesElemNum,
            inlinedFileInodesStart, outList);
      }

      // getters & setters
      int64_t getNewHashDirOffset() const
      {
         return newHashDirOffset;
      }

      int64_t getNewContDirOffset() const
      {
         return newContDirOffset;
      }

      std::string getCurrentContDirID() const
      {
         return currentContDirID;
      }
};

#endif /*RETRIEVEDIRENTRIESRESPMSG_H*/

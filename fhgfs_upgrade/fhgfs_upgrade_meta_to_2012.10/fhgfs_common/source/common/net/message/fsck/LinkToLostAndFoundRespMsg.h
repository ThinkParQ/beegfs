#ifndef LINKTOLOSTANDFOUNDRESPMSG_H
#define LINKTOLOSTANDFOUNDRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class LinkToLostAndFoundRespMsg : public NetMessage
{
   public:
      LinkToLostAndFoundRespMsg(FsckDirInodeList* failedDirInodes,
         FsckDirEntryList* createdDentries) : NetMessage(NETMSGTYPE_LinkToLostAndFoundResp)
      {
         this->failedDirInodes = failedDirInodes;
         this->failedFileInodes = NULL;
         this->createdDentries = createdDentries;
         this->entryType = FsckDirEntryType_DIRECTORY;
      }

      LinkToLostAndFoundRespMsg(FsckFileInodeList* failedFileInodes,
         FsckDirEntryList* createdDentries): NetMessage(NETMSGTYPE_LinkToLostAndFoundResp)
      {
         this->failedFileInodes = failedFileInodes;
         this->failedDirInodes = NULL;
         this->createdDentries = createdDentries;
         this->entryType = FsckDirEntryType_REGULARFILE;
      }

      LinkToLostAndFoundRespMsg() : NetMessage(NETMSGTYPE_LinkToLostAndFoundResp)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         if (FsckDirEntryType_ISDIR(this->entryType))
         {
            return NETMSG_HEADER_LENGTH +
               Serialization::serialLenInt() + // entryType
               SerializationFsck::serialLenFsckDirInodeList(failedDirInodes) + // dirInodes
               SerializationFsck::serialLenFsckDirEntryList(createdDentries); // createdDentries
         }
         else
         {
            return NETMSG_HEADER_LENGTH +
               Serialization::serialLenInt() + // entryType
               SerializationFsck::serialLenFsckFileInodeList(failedFileInodes) + // fileInodes
               SerializationFsck::serialLenFsckDirEntryList(createdDentries); // createdDentries
         }
      }


   private:
      FsckDirEntryType entryType; // to indicate, whether dir inodes or file inodes should be
                                  // linked; important for serialization

      FsckDirInodeList* failedDirInodes;
      FsckFileInodeList* failedFileInodes;

      FsckDirEntryList* createdDentries;

      // for deserialization
      const char* inodesStart;
      unsigned inodesElemNum;
      unsigned inodesBufLen;

      const char* createdDentriesStart;
      unsigned createdDentriesElemNum;
      unsigned createdDentriesBufLen;

   public:
      void parseFailedInodes(FsckDirInodeList* outFailedDirInodes)
      {
         if (FsckDirEntryType_ISDIR(this->entryType))
            SerializationFsck::deserializeFsckDirInodeList(inodesElemNum, inodesStart,
               outFailedDirInodes);
      }

      void parseFailedInodes(FsckFileInodeList* outFailedFileInodes)
      {
         if (! FsckDirEntryType_ISDIR(this->entryType))
            SerializationFsck::deserializeFsckFileInodeList(inodesElemNum, inodesStart,
               outFailedFileInodes);
      }

      void parseCreatedDirEntries(FsckDirEntryList* outCreatedDentries)
      {
         SerializationFsck::deserializeFsckDirEntryList(createdDentriesElemNum,
            createdDentriesStart, outCreatedDentries);
      }
};


#endif /*LINKTOLOSTANDFOUNDRESPMSG_H*/

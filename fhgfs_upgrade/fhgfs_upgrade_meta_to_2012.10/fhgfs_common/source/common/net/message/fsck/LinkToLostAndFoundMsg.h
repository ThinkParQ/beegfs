#ifndef LINKTOLOSTANDFOUNDMSG_H
#define LINKTOLOSTANDFOUNDMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/serialization/SerializationFsck.h>

class LinkToLostAndFoundMsg : public NetMessage
{
   public:
      LinkToLostAndFoundMsg(EntryInfo* lostAndFoundInfo, FsckDirInodeList* dirInodes) :
         NetMessage(NETMSGTYPE_LinkToLostAndFound)
      {
         this->lostAndFoundInfoPtr = lostAndFoundInfo;
         this->dirInodes = dirInodes;
         this->fileInodes = NULL;
         this->entryType = FsckDirEntryType_DIRECTORY;
      }

      LinkToLostAndFoundMsg(EntryInfo* lostAndFoundInfo, FsckFileInodeList* fileInodes) :
         NetMessage(NETMSGTYPE_LinkToLostAndFound)
      {
         this->lostAndFoundInfoPtr = lostAndFoundInfo;
         this->fileInodes = fileInodes;
         this->dirInodes = NULL;
         this->entryType = FsckDirEntryType_REGULARFILE;
      }

   protected:
      LinkToLostAndFoundMsg() : NetMessage(NETMSGTYPE_LinkToLostAndFound)
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
               lostAndFoundInfoPtr->serialLen() + // lostAndFoundInfo
               SerializationFsck::serialLenFsckDirInodeList(dirInodes); // dirInodes
         }
         else
         {
            return NETMSG_HEADER_LENGTH +
               Serialization::serialLenInt() + // entryType
               lostAndFoundInfoPtr->serialLen() + // lostAndFoundInfo
               SerializationFsck::serialLenFsckFileInodeList(fileInodes); // fileInodes
         }
      }


   private:
      FsckDirEntryType entryType; // to indicate, whether dir inodes or file inodes should be
                                  // linked; important for serialization

      // for serialization
      FsckDirInodeList* dirInodes; // not owned by this object
      FsckFileInodeList* fileInodes; // not owned by this object

      EntryInfo* lostAndFoundInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo lostAndFoundInfo;

      // for deserialization
      const char* inodesStart;
      unsigned inodesElemNum;
      unsigned inodesBufLen;

   public:
      void parseInodes(FsckDirInodeList* outDirInodes)
      {
         if ( FsckDirEntryType_ISDIR(this->entryType))
            SerializationFsck::deserializeFsckDirInodeList(inodesElemNum, inodesStart,
               outDirInodes);
      }

      void parseInodes(FsckFileInodeList* outFileInodes)
      {
         if ( !FsckDirEntryType_ISDIR(this->entryType))
            SerializationFsck::deserializeFsckFileInodeList(inodesElemNum, inodesStart,
               outFileInodes);
      }

      EntryInfo* getLostAndFoundInfo()
      {
         return &(this->lostAndFoundInfo);
      }

      FsckDirEntryType getEntryType()
      {
         return this->entryType;
      }
};


#endif /*LINKTOLOSTANDFOUNDMSG_H*/

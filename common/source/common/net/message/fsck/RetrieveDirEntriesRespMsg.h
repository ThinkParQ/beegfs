#ifndef RETRIEVEDIRENTRIESRESPMSG_H
#define RETRIEVEDIRENTRIESRESPMSG_H

#include <common/Common.h>
#include <common/fsck/FsckContDir.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckFileInode.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/ListTk.h>

class RetrieveDirEntriesRespMsg : public NetMessageSerdes<RetrieveDirEntriesRespMsg>
{
   public:
      RetrieveDirEntriesRespMsg(FsckContDirList* fsckContDirs, FsckDirEntryList* fsckDirEntries,
         FsckFileInodeList* inlinedFileInodes, std::string& currentContDirID,
         int64_t newHashDirOffset, int64_t newContDirOffset) :
            BaseType(NETMSGTYPE_RetrieveDirEntriesResp)
      {
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->newHashDirOffset = newHashDirOffset;
         this->newContDirOffset = newContDirOffset;
         this->fsckContDirs = fsckContDirs;
         this->fsckDirEntries = fsckDirEntries;
         this->inlinedFileInodes = inlinedFileInodes;
      }

      RetrieveDirEntriesRespMsg() : BaseType(NETMSGTYPE_RetrieveDirEntriesResp)
      {
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
      struct {
         FsckContDirList fsckContDirs;
         FsckDirEntryList fsckDirEntries;
         FsckFileInodeList inlinedFileInodes;
      } parsed;

   public:
      FsckContDirList& getContDirs()
      {
         return *fsckContDirs;
      }

      FsckDirEntryList& getDirEntries()
      {
         return *fsckDirEntries;
      }

      FsckFileInodeList& getInlinedFileInodes()
      {
         return *inlinedFileInodes;
      }

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

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->currentContDirID, obj->currentContDirIDLen, 4)
            % obj->newHashDirOffset
            % obj->newContDirOffset
            % serdes::backedPtr(obj->fsckContDirs, obj->parsed.fsckContDirs)
            % serdes::backedPtr(obj->fsckDirEntries, obj->parsed.fsckDirEntries)
            % serdes::backedPtr(obj->inlinedFileInodes, obj->parsed.inlinedFileInodes);
      }
};

#endif /*RETRIEVEDIRENTRIESRESPMSG_H*/

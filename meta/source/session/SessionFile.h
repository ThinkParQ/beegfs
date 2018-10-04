#ifndef SESSIONFILE_H_
#define SESSIONFILE_H_

#include <common/storage/Path.h>
#include <common/Common.h>
#include <storage/DirInode.h>
#include <storage/FileInode.h>
#include <storage/MetaFileHandle.h>

class MetaStore;

class SessionFile
{
   public:
      /**
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      SessionFile(MetaFileHandle openInode, unsigned accessFlags, EntryInfo* entryInfo):
         inode(std::move(openInode)), accessFlags(accessFlags), sessionID(0),
         useAsyncCleanup(false)
      {
         this->entryInfo.set(entryInfo);
      }

      /**
       * For dezerialisation only
       */
      SessionFile():
         accessFlags(0), sessionID(0), useAsyncCleanup(false)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->accessFlags
            % obj->sessionID
            % obj->entryInfo
            % obj->useAsyncCleanup;
      }

      bool relinkInode(MetaStore& store);

      bool operator==(const SessionFile& other) const;

      bool operator!=(const SessionFile& other) const { return !(*this == other); }

   private:
      MetaFileHandle inode;
      uint32_t accessFlags; // OPENFILE_ACCESS_... flags
      uint32_t sessionID;
      bool useAsyncCleanup; // if session was busy (=> referenced) while client sent close

      EntryInfo entryInfo;

      // getters & setters

      /**
       * Returns lock of internal FileInode object.
       */
      RWLock* getInodeLock()
      {
         return &inode->rwlock;
      }

   public:

      // getters & setters
      unsigned getAccessFlags()
      {
         return accessFlags;
      }

      unsigned getSessionID()
      {
         return sessionID;
      }

      void setSessionID(unsigned sessionID)
      {
         this->sessionID = sessionID;
      }

      const MetaFileHandle& getInode() const
      {
         return inode;
      }

      MetaFileHandle releaseInode()
      {
         return std::move(inode);
      }

      bool getUseAsyncCleanup()
      {
         // note: unsynced, because it can only become true and stays true then
         return this->useAsyncCleanup;
      }

      void setUseAsyncCleanup()
      {
         // note: unsynced, because it can only become true and stays true then
         this->useAsyncCleanup = true;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      /**
       * Update the parentEntryID
       */
      void setParentEntryID(const std::string& newParentID)
      {
         this->entryInfo.setParentEntryID(newParentID);
      }
};



#endif /*SESSIONFILE_H_*/

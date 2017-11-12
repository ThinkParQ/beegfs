#ifndef SESSIONFILE_H_
#define SESSIONFILE_H_

#include <common/storage/Path.h>
#include <common/threading/SafeMutexLock.h>
#include <common/Common.h>
#include <storage/DirInode.h>
#include <storage/FileInode.h>


class SessionFile
{
   public:
      /**
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      SessionFile(FileInode* openInode, unsigned accessFlags, EntryInfo* entryInfo)
      {
         this->inode = openInode;
         this->accessFlags = accessFlags;
         this->entryInfo.set(entryInfo);

         // there is no value to represent an invalid/unassigned ID, but that's not a problem
         this->sessionID = 0;

         this->useAsyncCleanup = false;
      }


   private:
      FileInode* inode;
      unsigned accessFlags; // OPENFILE_ACCESS_... flags
      unsigned sessionID;
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

      FileInode* getInode()
      {
         return inode;
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
      void setParentEntryID(std::string& newParentID)
      {
         this->entryInfo.setParentEntryID(newParentID);
      }

};



#endif /*SESSIONFILE_H_*/

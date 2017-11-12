#include <common/threading/SafeMutexLock.h>
#include <program/Program.h>
#include "SessionFileStore.h"

/**
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 * @return assigned sessionID
 */
unsigned SessionFileStore::addSession(SessionFile* session)
{
   SafeMutexLock mutexLock(&mutex);

   unsigned sessionID = generateNewSessionID();
   session->setSessionID(sessionID);
   
   sessions.insert(SessionFileMapVal(sessionID, new SessionFileReferencer(session) ) );

   mutexLock.unlock();
   
   return sessionID;
}

/**
 * Insert a session with a pre-defined sessionID and reference it. Will fail if given ID existed
 * already.
 *
 * Note: This is a special method intended for recovery purposes only!
 * Note: remember to call releaseSession() if the new session was actually inserted.
 *
 * @param session belongs to the store if it was inserted - so do not free it and don't
 * use it any more afterwards (reference it if you still need it)
 * @return NULL if sessionID existed already (and new session not inserted or touched in any way),
 * pointer to referenced session file otherwise.
 */
SessionFile* SessionFileStore::addAndReferenceRecoverySession(SessionFile* session)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   SessionFile* retVal = NULL;
   unsigned sessionID = session->getSessionID();

   SessionFileMapIter iter = sessions.find(sessionID);
   if(iter == sessions.end() )
   { // session with this ID didn't exist (this is the normal case) => insert and reference it

      /* note: we do the sessions.find() first to avoid allocating SessionRefer if it's not
         necessary */

      SessionFileReferencer* sessionFileRefer = new SessionFileReferencer(session);

      sessions.insert(SessionFileMapVal(sessionID, sessionFileRefer) );

      retVal = sessionFileRefer->reference();
   }

   mutexLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Note: remember to call releaseSession()
 * 
 * @return NULL if no such session exists
 */
SessionFile* SessionFileStore::referenceSession(unsigned sessionID)
{
   SessionFile* session;
   
   SafeMutexLock mutexLock(&mutex);
   
   SessionFileMapIter iter = sessions.find(sessionID);
   if(iter == sessions.end() )
   { // not found
      session = NULL;
   }
   else
   {
      SessionFileReferencer* sessionRefer = iter->second;
      session = sessionRefer->reference();
   }
   
   mutexLock.unlock();
   
   return session;
}

void SessionFileStore::releaseSession(SessionFile* session, EntryInfo* entryInfo)
{
   unsigned sessionID = session->getSessionID();
   bool asyncCleanup = false;
   unsigned asyncCleanupAccessFlags = 0; // only for asyncCleanup
   FileInode* asyncCleanupFile = NULL; // only for asyncCleanup

   SafeMutexLock mutexLock(&mutex); // L O C K
   
   SessionFileMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   { // session exists => decrease refCount
      SessionFileReferencer* sessionRefer = iter->second;
      SessionFile* sessionNonRef = sessionRefer->getReferencedObject();

      if(unlikely(sessionNonRef->getUseAsyncCleanup() ) )
      { // marked for async cleanup => check whether we're dropping the last reference
         if(sessionRefer->getRefCount() == 1)
         { // we're dropping the last reference => save async cleanup data and trigger cleanup

            asyncCleanup = true;

            asyncCleanupFile = sessionNonRef->getInode();
            asyncCleanupAccessFlags = sessionNonRef->getAccessFlags();

            sessionRefer->release();

            sessions.erase(iter);
            delete(sessionRefer);
         }
      }
      else
      { // the normal case: just release this reference
         sessionRefer->release();
      }
   }
   
   mutexLock.unlock(); // U N L O C K


   if(unlikely(asyncCleanup) )
      performAsyncCleanup(entryInfo, asyncCleanupFile, asyncCleanupAccessFlags);
}

/**
 * @return false if session could not be removed, because it was still in use; it will be marked
 * for async cleanup when the last reference is dropped
 */
bool SessionFileStore::removeSession(unsigned sessionID)
{
   bool delErr = true;
   
   SafeMutexLock mutexLock(&mutex);
   
   SessionFileMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   {
      SessionFileReferencer* sessionRefer = iter->second;
      
      if(sessionRefer->getRefCount() )
      { // session still in use => mark for async cleanup (on last reference drop)
         SessionFile* fileNonRef = sessionRefer->getReferencedObject();

         fileNonRef->setUseAsyncCleanup();

         delErr = true;
      }
      else
      { // no references => delete
         sessions.erase(iter);
         delete(sessionRefer);
         delErr = false;
      }
   }
   
   mutexLock.unlock();
   
   return !delErr;
}

/**
 * @return might be NULL if the session is in use
 */
SessionFile* SessionFileStore::removeAndGetSession(unsigned sessionID)
{
   // note: this method is currently unused

   SessionFile* session = NULL;
   
   SafeMutexLock mutexLock(&mutex);
   
   SessionFileMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   {
      SessionFileReferencer* sessionRefer = iter->second;
      
      if(!sessionRefer->getRefCount() )
      { // no references => allow deletion
         sessions.erase(iter);
         sessionRefer->setOwnReferencedObject(false);
         session = sessionRefer->getReferencedObject();
         delete(sessionRefer);
      }
   }
   
   mutexLock.unlock();
   
   return session;
}

/**
 * Removes all sessions and additionally adds those that had a reference count to the StringList.
 * 
 * @outRemovedSessions caller is responsible for clean up of contained objects
 */
void SessionFileStore::removeAllSessions(SessionFileList* outRemovedSessions,
   UIntList* outReferencedSessions)
{
   SafeMutexLock mutexLock(&mutex);
   
   for(SessionFileMapIter iter = sessions.begin(); iter != sessions.end(); iter++)
   {
      SessionFileReferencer* sessionRefer = iter->second;
      SessionFile* session = sessionRefer->getReferencedObject();
      
      outRemovedSessions->push_back(session);
      
      if(unlikely(sessionRefer->getRefCount() ) )
         outReferencedSessions->push_back(iter->first);

      sessionRefer->setOwnReferencedObject(false);
      delete(sessionRefer);
   }
   
   sessions.clear();

   mutexLock.unlock();
}

size_t SessionFileStore::getSize()
{
   SafeMutexLock mutexLock(&mutex);
   
   size_t sessionsSize = sessions.size();

   mutexLock.unlock();

   return sessionsSize;
}


unsigned SessionFileStore::generateNewSessionID()
{
   SessionFileMapIter iter;
   
   // note: we assume here that there always is at least one free sessionID.

   do
   {
      // generate new ID
      lastSessionID++;
      
      // check whether this ID is being used already
      iter = sessions.find(lastSessionID);
      
   } while(iter != sessions.end() );
   
   // we found an available sessionID => return it
   
   return lastSessionID;
}

/**
 * Performs local cleanup tasks for file sessions, which are marked for async cleanup.
 */
void SessionFileStore::performAsyncCleanup(EntryInfo* entryInfo,
   FileInode* inode, unsigned accessFlags)
{
   const char* logContext = __func__;
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   unsigned numHardlinks;
   unsigned numInodeRefs;

   LOG_DEBUG(logContext, Log_NOTICE, "Performing async cleanup of file session");
   IGNORE_UNUSED_VARIABLE(logContext);

   metaStore->closeFile(entryInfo, inode, accessFlags, &numHardlinks, &numInodeRefs);

   /* note: we ignore closing storage server files here (e.g. because we don't have the sessionID
      and fileHandleID at hand) and unlinking of disposable files (disposal can still be triggered
      by fhgfs_online_cfg).
      this is something that we should change in the future (but maybe rather indirectly by syncing
      open files between clients and servers at regular intervals). */
}




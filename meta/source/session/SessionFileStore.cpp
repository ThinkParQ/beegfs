#include <program/Program.h>
#include "SessionFileStore.h"

#include <mutex>

/**
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 * @return assigned sessionID
 */
unsigned SessionFileStore::addSession(SessionFile* session)
{
   const std::lock_guard<Mutex> lock(mutex);

   unsigned sessionID = generateNewSessionID();
   session->setSessionID(sessionID);

   sessions.insert(SessionFileMapVal(sessionID, new SessionFileReferencer(session) ) );

   return sessionID;
}

/**
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 * @param sessionFileID use this ID to store session file
 * @return true if successfully added, false otherwise (most likely ID conflict)
 */
bool SessionFileStore::addSession(SessionFile* session, unsigned sessionFileID)
{
   const std::lock_guard<Mutex> lock(mutex);

   session->setSessionID(sessionFileID);

   std::pair<SessionFileMapIter, bool> insertRes =
      sessions.insert(SessionFileMapVal(sessionFileID, new SessionFileReferencer(session) ) );

   return insertRes.second;
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
   const std::lock_guard<Mutex> lock(mutex);

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

   return retVal;
}

/**
 * Note: remember to call releaseSession()
 * 
 * @return NULL if no such session exists
 */
SessionFile* SessionFileStore::referenceSession(unsigned sessionID)
{
   const std::lock_guard<Mutex> lock(mutex);

   SessionFileMapIter iter = sessions.find(sessionID);
   if(iter == sessions.end() )
   { // not found
      return NULL;
   }
   else
   {
      SessionFileReferencer* sessionRefer = iter->second;
      return sessionRefer->reference();
   }
}

void SessionFileStore::releaseSession(SessionFile* session, EntryInfo* entryInfo)
{
   unsigned sessionID = session->getSessionID();
   bool asyncCleanup = false;
   unsigned asyncCleanupAccessFlags = 0; // only for asyncCleanup
   MetaFileHandle asyncCleanupFile; // only for asyncCleanup

   {
      const std::lock_guard<Mutex> lock(mutex);

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

               asyncCleanupFile = sessionNonRef->releaseInode();
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

   }

   if(unlikely(asyncCleanup) )
      performAsyncCleanup(entryInfo, std::move(asyncCleanupFile), asyncCleanupAccessFlags);
}

/**
 * @return false if session could not be removed, because it was still in use; it will be marked
 * for async cleanup when the last reference is dropped
 */
bool SessionFileStore::removeSession(unsigned sessionID)
{
   bool delErr = true;

   const std::lock_guard<Mutex> lock(mutex);

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

   return !delErr;
}

/**
 * @return might be NULL if the session is in use
 */
SessionFile* SessionFileStore::removeAndGetSession(unsigned sessionID)
{
   // note: this method is currently unused

   SessionFile* session = NULL;

   const std::lock_guard<Mutex> lock(mutex);

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
   const std::lock_guard<Mutex> lock(mutex);

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
}

/*
 * Intended to be used for cleanup if deserialization failed, no locking is used
 */
void SessionFileStore::deleteAllSessions()
{
   for(SessionFileMapIter iter = sessions.begin(); iter != sessions.end(); iter++)
   {
      SessionFileReferencer* sessionRefer = iter->second;

      delete(sessionRefer);
   }

   sessions.clear();
}

size_t SessionFileStore::getSize()
{
   const std::lock_guard<Mutex> lock(mutex);

   return sessions.size();
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
   MetaFileHandle inode, unsigned accessFlags)
{
   const char* logContext = __func__;
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   unsigned numHardlinks;
   unsigned numInodeRefs;

   LOG_DEBUG(logContext, Log_NOTICE, "Performing async cleanup of file session");
   IGNORE_UNUSED_VARIABLE(logContext);

   metaStore->closeFile(entryInfo, std::move(inode), accessFlags, &numHardlinks, &numInodeRefs);

   /* note: we ignore closing storage server files here (e.g. because we don't have the sessionID
      and fileHandleID at hand) and unlinking of disposable files (disposal can still be triggered
      by fhgfs_online_cfg).
      this is something that we should change in the future (but maybe rather indirectly by syncing
      open files between clients and servers at regular intervals). */
}

SessionFileMap* SessionFileStore::getSessionMap()
{
   return &this->sessions;
}

/* Merges the SessionFiles of the given SessionFileStore into this SessionFileStore.
 * Only not existing SessionFiles will be added to the existing SessionFileStore
 *
 * @param sessionFileStore the sessionFileStore which will be merged with this sessionFileStore
 */
void SessionFileStore::mergeSessionFiles(SessionFileStore* sessionFileStore)
{
   Logger* log = Logger::getLogger();

   SessionFileMapIter sessionIter = sessionFileStore->getSessionMap()->begin();

   while(sessionIter != sessionFileStore->getSessionMap()->end() )
   {
      bool sessionFound = false;
      SessionFileMapIter destSessionIter = this->sessions.find(sessionIter->first);

      if (destSessionIter != this->sessions.end())
      {
            sessionFound = true;
            log->log(Log_WARNING, "SessionFileStore merge", "found SessionFile with same "
               "ID: " + StringTk::uintToStr(sessionIter->first) +
               " , merge not possible, may be a bug?");
      }

      if (!sessionFound)
      {
         bool success = this->sessions.insert(SessionFileMapVal(sessionIter->first,
            sessionIter->second)).second;

         if (!success)
         {
            log->log(Log_WARNING, "SessionFileStore merge", "could not merge: " +
               StringTk::uintToStr(sessionIter->first) );

            delete(sessionIter->second);
         }
      }
      else
      {
         delete(sessionIter->second);
      }

      sessionIter++;
   }
}

bool SessionFileStore::operator==(const SessionFileStore& other) const
{
   struct ops {
      static bool cmp(const SessionFileMapVal& lhs, const SessionFileMapVal& rhs)
      {
         return lhs.first == rhs.first
            && *lhs.second->getReferencedObject() == *rhs.second->getReferencedObject();
      }
   };

   return lastSessionID == other.lastSessionID
      && sessions.size() == other.sessions.size()
      && std::equal(
            sessions.begin(), sessions.end(),
            other.sessions.begin(),
            ops::cmp);
}

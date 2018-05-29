#include <app/App.h>
#include <common/threading/SafeMutexLock.h>
#include <program/Program.h>
#include "SessionLocalFileStore.h"


#define SESSIONLOCALFILESTORE_MIRROR_KEY_SUFFIX    ".m" /* added to handles for mirror map keys */


/**
 * Add a new session to the store (if it doesn't exist yet) and return a referenced version
 * of the session with this ID. Thus, releaseSession() must definitely be called later.
 *
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (use the returned pointer instead)
 */
SessionLocalFile* SessionLocalFileStore::addAndReferenceSession(SessionLocalFile* session)
{
   std::string fileHandleID(session->getFileHandleID() );
   uint16_t targetID = session->getTargetID();
   bool isMirrorSession = session->getIsMirrorSession();

   std::string key(generateMapKey(fileHandleID, targetID, isMirrorSession) );

   SafeMutexLock mutexLock(&mutex); // L O C K
   
   // try to insert the new session
   std::pair<SessionLocalFileMapIter, bool> insertRes =
      sessions.insert(SessionLocalFileMapVal(key, new SessionLocalFileReferencer(session) ) );

   // was a session with this ID in the store already?
   if(!insertRes.second)
   { // session exists => discard new session
      delete(session);
   }

   // reference session (note: insertRes.first is an iterator to the inserted/existing session)
   SessionLocalFileReferencer* sessionRefer = insertRes.first->second;
   SessionLocalFile* retVal = sessionRefer->reference();

   mutexLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Note: remember to call releaseSession()
 * 
 * @param targetID really targetID (not buddy group ID for mirrors, because both buddies can be
 *        attached to the same server).
 * @param isMirrorSession true if this is a session for a mirrored chunk file.
 * @return NULL if no such session exists.
 */
SessionLocalFile* SessionLocalFileStore::referenceSession(const std::string fileHandleID,
   uint16_t targetID, bool isMirrorSession)
{
   SessionLocalFile* session;
   
   std::string key(generateMapKey(fileHandleID, targetID, isMirrorSession) );

   SafeMutexLock mutexLock(&mutex);

   SessionLocalFileMapIter iter = sessions.find(key);
   if(iter == sessions.end() )
   { // not found
      session = NULL;
   }
   else
   {
      SessionLocalFileReferencer* sessionRefer = iter->second;
      session = sessionRefer->reference();
   }
   
   mutexLock.unlock();
   
   return session;
}

void SessionLocalFileStore::releaseSession(SessionLocalFile* session)
{
   std::string fileHandleID(session->getFileHandleID() );
   uint16_t targetID = session->getTargetID();
   bool isMirrorSession = session->getIsMirrorSession();

   std::string key(generateMapKey(fileHandleID, targetID, isMirrorSession) );

   SafeMutexLock mutexLock(&mutex); // L O C K

   SessionLocalFileMapIter iter = sessions.find(key);
   if(iter != sessions.end() )
   { // session exists => decrease refCount
      SessionLocalFileReferencer* sessionRefer = iter->second;
      SessionLocalFile* fileNonRef = sessionRefer->getReferencedObject();

      sessionRefer->release();

      // check delayed removal
      if(!sessionRefer->getRefCount() && fileNonRef->getRemoveOnRelease() )
      {
         /* special case: last ref dropped and delayed session removal is set
            => cleanup internally and remove session */

         // close fd
         if(fileNonRef->getFD() != -1)
            close(fileNonRef->getFD() );

         sessions.erase(iter);
         delete(sessionRefer);
      }
   }
   
   mutexLock.unlock(); // U N L O C K
}

/**
 * @param isMirrorSession true if this is a session for a mirror chunk file
 * @param outSessionLocalFile removed session if success was returned and the session existed,
 * NULL otherwise; caller is responsible for deletion of this object.
 * @return true if session was successfully removed or didn't exist, false if session was still in
 * use (and thus was marked for delayed removal internally).
 */
bool SessionLocalFileStore::removeSession(const std::string fileHandleID,
   uint16_t targetID, bool isMirrorSession, SessionLocalFile** outSessionLocalFile)
{
   bool sessionWasReferenced = false; // non-existing session doesn't indicate an error
   
   *outSessionLocalFile = NULL;

   std::string key(generateMapKey(fileHandleID, targetID, isMirrorSession) );


   SafeMutexLock mutexLock(&mutex); // L O C K
   
   SessionLocalFileMapIter iter = sessions.find(key);
   if(iter != sessions.end() )
   { // session found

      SessionLocalFileReferencer* sessionRefer = iter->second;
      SessionLocalFile* fileNonRef = sessionRefer->getReferencedObject();
      
      if(sessionRefer->getRefCount() )
      { // unable to remove now => mark for delayed removal on last reference drop
         fileNonRef->setRemoveOnRelease(true);
         sessionWasReferenced = true;
      }
      else
      { // no references => remove from store
         *outSessionLocalFile = fileNonRef;
         
         sessions.erase(iter);

         sessionRefer->setOwnReferencedObject(false);
         delete(sessionRefer);
      }
   }
   
   mutexLock.unlock(); // U N L O C K
   
   return !sessionWasReferenced;
}

/**
 * Removes all sessions and additionally adds those that had a reference count to the StringList.
 * 
 * @param outRemovedSessions caller is responsible for clean up of contained objects
 */
void SessionLocalFileStore::removeAllSessions(SessionLocalFileList* outRemovedSessions,
   StringList* outReferencedSessions)
{
   SafeMutexLock mutexLock(&mutex);
   
   for(SessionLocalFileMapIter iter = sessions.begin(); iter != sessions.end(); iter++)
   {
      SessionLocalFileReferencer* sessionRefer = iter->second;
      SessionLocalFile* session = sessionRefer->getReferencedObject();
      
      outRemovedSessions->push_back(session);

      if(unlikely(sessionRefer->getRefCount() ) )
         outReferencedSessions->push_back(iter->first);

      sessionRefer->setOwnReferencedObject(false);
      delete(sessionRefer);
   }
   
   sessions.clear();

   mutexLock.unlock();
}

/**
 * Removes all mirror sessions on a specific target.
 */
void SessionLocalFileStore::removeAllMirrorSessions(uint16_t targetID)
{
   SafeMutexLock mutexLock(&mutex);

   StringList fileHandleIDs;

   SessionLocalFileMapIter iter = sessions.begin();
   while(iter != sessions.end())
   {
      SessionLocalFileReferencer* sessionRefer = iter->second;
      SessionLocalFile* session = sessionRefer->getReferencedObject();

      if ( (session->getTargetID() == targetID) && (session->getIsMirrorSession()) )
      {
         if(sessionRefer->getRefCount())
         { // unable to remove now => mark for delayed removal on last reference drop
            session->setRemoveOnRelease(true);
         }
         else
         { // no references => remove from store
            sessions.erase(iter++);
            sessionRefer->setOwnReferencedObject(true);
            delete(sessionRefer);
            continue;
         }
      }

      ++iter;
   }

   mutexLock.unlock();
}

/*
 * Intended to be used for cleanup if deserialization failed, no locking is used
 */
void SessionLocalFileStore::deleteAllSessions()
{
   for(SessionLocalFileMapIter iter = sessions.begin(); iter != sessions.end(); iter++)
   {
      SessionLocalFileReferencer* sessionRefer = iter->second;

      delete(sessionRefer);
   }

   sessions.clear();
}

size_t SessionLocalFileStore::getSize()
{
   SafeMutexLock mutexLock(&mutex);
   
   size_t sessionsSize = sessions.size();

   mutexLock.unlock();

   return sessionsSize;
}

/**
 * Note: if changes are made, check SessionLocalFileStore::getTargetIDFromKey to ensure it still
 *       works.
 *
 * @param targetID really targetID (not buddy group ID for mirrors, because both buddies can be
 *        attached to the same server).
 * @param isMirrorSession true if this is a session for a mirror chunk file (has an influence on
 * the map key to avoid conflicts with the original session in rotated mirrors mode).
 */
std::string SessionLocalFileStore::generateMapKey(const std::string fileHandleID,
   uint16_t targetID, bool isMirrorSession)
{
   if(isMirrorSession)
      return fileHandleID + SESSIONLOCALFILESTORE_MIRROR_KEY_SUFFIX "@" +
         StringTk::uintToHexStr(targetID);
   else
      return fileHandleID + "@" + StringTk::uintToHexStr(targetID);
}

/*
 * Note: if changes are made, check SessionLocalFileStore::generateMapKey to ensure it's still
 *       compatible with this method.
 *
 * @return 0 if key could not be parsed
 */
uint16_t SessionLocalFileStore::getTargetIDFromKey(std::string key)
{
   StringVector vec;
   StringTk::explode(key, '@', &vec);

   if (vec.size() == 2)
      return StringTk::strHexToUInt(vec[1]);

   return 0;
}

SessionLocalFileMap* SessionLocalFileStore::getSessionMap()
{
   return &this->sessions;
}

/* Merges the SessionLocalFiles of the given SessionLocalFileStore into this SessionLocalFileStore.
 * Only not existing SessionLocalFiles will be added to the existing SessionLocalFileStore
 *
 * @param sessionLocalFileStore the sessionLocalFileStore which will be merged with this
 * sessionLocalFileStore
 */
void SessionLocalFileStore::mergeSessionLocalFiles(SessionLocalFileStore* sessionLocalFileStore)
{
   Logger* log = Logger::getLogger();

   SessionLocalFileMapIter sessionIter = sessionLocalFileStore->getSessionMap()->begin();

   while(sessionIter != sessionLocalFileStore->getSessionMap()->end() )
   {
      bool sessionFound = false;
      SessionLocalFileMapIter destSessionIter = this->sessions.find(sessionIter->first);

      if (destSessionIter != this->sessions.end())
      {
            sessionFound = true;
            log->log(Log_WARNING, "SessionLocalFileStore merge", "found SessionLocalFile with same "
               "ID: " + sessionIter->first + " , merge not possible, may be a bug?");
      }

      if (!sessionFound)
      {
         bool success = this->sessions.insert(SessionLocalFileMapVal(sessionIter->first,
            sessionIter->second)).second;

         if (!success)
         {
            log->log(Log_WARNING, "SessionLocalFileStore merge", "could not merge: " +
               std::string(sessionIter->first));

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

void SessionLocalFileStore::serializeForTarget(Serializer& ser, uint16_t targetID)
{
   Serializer atStart = ser.mark();

   uint32_t elemCount = 0;
   ser % elemCount; // needs fixup

   for (SessionLocalFileMapIter it = sessions.begin(), end = sessions.end(); it != end; ++it)
   {
      if (getTargetIDFromKey(it->first) != targetID)
         continue;

      ser
         % it->first
         % *it->second->getReferencedObject();
      elemCount++;
   }

   atStart % elemCount;

   LOG_DEBUG("SessionLocalFileStore serialize", Log_DEBUG, "count of serialized "
      "SessionLocalFiles: " + StringTk::uintToStr(elemCount) + " of " +
      StringTk::uintToStr(sessions.size()));
}

void SessionLocalFileStore::deserializeForTarget(Deserializer& des, uint16_t targetID)
{
   uint32_t elemCount;

   des % elemCount;
   if (unlikely(!des.good()))
      return;

   for (uint32_t i = 0; i < elemCount; i++)
   {
      std::string key;
      SessionLocalFile* sessionLocalFile = new SessionLocalFile();

      des
         % key
         % *sessionLocalFile;

      if (unlikely(!des.good() || getTargetIDFromKey(key) != targetID))
      {
         delete sessionLocalFile;
         des.setBad();
         return;
      }

      this->sessions.insert(SessionLocalFileMapVal(key,
         new SessionLocalFileReferencer(sessionLocalFile)));
   }

   LOG_DEBUG("SessionLocalFileStore deserialize", Log_DEBUG, "count of deserialized "
      "SessionLocalFiles: " + StringTk::uintToStr(elemCount));
}

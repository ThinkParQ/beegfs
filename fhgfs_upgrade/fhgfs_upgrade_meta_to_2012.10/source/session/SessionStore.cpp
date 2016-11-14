#include <common/threading/SafeMutexLock.h>
#include "SessionStore.h"

/**
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
void SessionStore::addSession(Session* session)
{
   std::string sessionID = session->getSessionID();

   SafeMutexLock mutexLock(&mutex);
   
   // is session in the store already?
   
   SessionMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   {
      delete(session);
   }
   else
   { // session not in the store yet
      sessions.insert(SessionMapVal(sessionID, new SessionReferencer(session) ) );
   }

   mutexLock.unlock();

}

/**
 * Note: remember to call releaseSession()
 * 
 * @return NULL if no such session exists
 */
Session* SessionStore::referenceSession(std::string sessionID, bool addIfNotExists)
{
   Session* session;
   
   SafeMutexLock mutexLock(&mutex);
   
   SessionMapIter iter = sessions.find(sessionID);
   if(iter == sessions.end() )
   { // not found
      if(!addIfNotExists)
         session = NULL;
      else
      { // add as new session and reference it
         LogContext log("SessionStore (ref)");
         log.log(3, std::string("Creating a new session. SessionID: ") + sessionID);
         
         Session* newSession = new Session(sessionID);
         SessionReferencer* sessionRefer = new SessionReferencer(newSession);
         sessions.insert(SessionMapVal(sessionID, sessionRefer) );
         session = sessionRefer->reference();
      }
   }
   else
   {
      SessionReferencer* sessionRefer = iter->second;
      session = sessionRefer->reference();
   }
   
   mutexLock.unlock();
   
   return session;
}

void SessionStore::releaseSession(Session* session)
{
   std::string sessionID = session->getSessionID();

   SafeMutexLock mutexLock(&mutex);
   
   SessionMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   { // session exists => decrease refCount
      SessionReferencer* sessionRefer = iter->second;
      sessionRefer->release();
   }
   
   mutexLock.unlock();
}

bool SessionStore::removeSession(std::string sessionID)
{
   bool delErr = true;
   
   SafeMutexLock mutexLock(&mutex);
   
   SessionMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   {
      SessionReferencer* sessionRefer = iter->second;
      
      if(sessionRefer->getRefCount() )
         delErr = true;
      else
      { // no references => delete
         sessions.erase(sessionID);
         delete(sessionRefer);
         delErr = false;
      }
   }
   
   mutexLock.unlock();
   
   return !delErr;
}

/**
 * @return NULL if session is referenced, otherwise the sesion must be cleaned up by the caller
 */
Session* SessionStore::removeSessionUnlocked(std::string sessionID)
{
   Session* retVal = NULL;

   SessionMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   {
      SessionReferencer* sessionRefer = iter->second;
      
      if(!sessionRefer->getRefCount() )
      { // no references => return session to caller
         retVal = sessionRefer->getReferencedObject();
         
         sessionRefer->setOwnReferencedObject(false);
         delete(sessionRefer);
         sessions.erase(sessionID);
      }
   }
   
   return retVal;
}

/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param outRemovedSessions contained sessions must be cleaned up by the caller
 * @param outUnremovableSesssions contains sessions that would have been removed but are currently
 * referenced
 */
void SessionStore::syncSessions(NodeList* masterList, SessionList* outRemovedSessions,
   StringList* outUnremovableSesssions)
{
   SafeMutexLock mutexLock(&mutex);
   
   SessionMapIter sessionIter = sessions.begin();
   NodeListIter masterIter = masterList->begin();
   
   while( (sessionIter != sessions.end() ) && (masterIter != masterList->end() ) )
   {
      std::string currentSession(sessionIter->first);
      std::string currentMaster( (*masterIter)->getID() );

      if(currentMaster < currentSession)
      { // session is added
         // note: we add sessions only on demand, so we just delete the node object here
         delete(*masterIter);
         
         masterList->pop_front();
         masterIter = masterList->begin();
      }
      else
      if(currentSession < currentMaster)
      { // session is removed
         sessionIter++; // (removal invalidates iterator)

         Session* session = removeSessionUnlocked(currentSession);
         if(session)
            outRemovedSessions->push_back(session);
         else
            outUnremovableSesssions->push_back(currentSession); // session was referenced
      }
      else
      { // session unchanged
         delete(*masterIter);

         masterList->pop_front();
         masterIter = masterList->begin();

         sessionIter++;
      }
   }
   
   // remaining masterList entries are new
   while(masterIter != masterList->end() )
   {
      // note: we add sessions only on demand, so we just delete the node object here
      delete(*masterIter);
      
      masterList->pop_front();
      masterIter = masterList->begin();
   }

   // remaining sessions are removed
   while(sessionIter != sessions.end() )
   {
      std::string currentSession(sessionIter->first);
      sessionIter++; // (removal invalidates iterator)

      Session* session = removeSessionUnlocked(currentSession);
      if(session)
         outRemovedSessions->push_back(session);
      else
         outUnremovableSesssions->push_back(currentSession); // session was referenced
   }
   
   
   mutexLock.unlock();
}

/**
 * @return number of sessions
 */
size_t SessionStore::getAllSessionIDs(StringList* outSessionIDs)
{
   SafeMutexLock mutexLock(&mutex);

   size_t retVal = sessions.size();

   for(SessionMapIter iter = sessions.begin(); iter != sessions.end(); iter++)
      outSessionIDs->push_back(iter->first);

   mutexLock.unlock();

   return retVal;
}

size_t SessionStore::getSize()
{
   SafeMutexLock mutexLock(&mutex);
   
   size_t sessionsSize = sessions.size();

   mutexLock.unlock();

   return sessionsSize;
}









#include "common/threading/SafeMutexLock.h"
#include "SessionStore.h"

#include <boost/scoped_array.hpp>


/**
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
void SessionStore::addSession(Session* session)
{
   NumNodeID sessionID = session->getSessionID();

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
Session* SessionStore::referenceSession(NumNodeID sessionID, bool addIfNotExists)
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
         log.log(Log_DEBUG, std::string("Creating a new session. SessionID: ") + sessionID.str());
         
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
   NumNodeID sessionID = session->getSessionID();

   SafeMutexLock mutexLock(&mutex);
   
   SessionMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   { // session exists => decrease refCount
      SessionReferencer* sessionRefer = iter->second;
      sessionRefer->release();
   }
   
   mutexLock.unlock();
}

bool SessionStore::removeSession(NumNodeID sessionID)
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
Session* SessionStore::removeSessionUnlocked(NumNodeID sessionID)
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
void SessionStore::syncSessions(const std::vector<NodeHandle>& masterList,
   SessionList* outRemovedSessions, NumNodeIDList* outUnremovableSesssions)
{
   SafeMutexLock mutexLock(&mutex);

   SessionMapIter sessionIter = sessions.begin();
   auto masterIter = masterList.begin();

   while (sessionIter != sessions.end() && masterIter != masterList.end())
   {
      NumNodeID currentSession = sessionIter->first;
      NumNodeID currentMaster = (*masterIter)->getNumID();

      if(currentMaster < currentSession)
      { // session doesn't exist locally
         // note: we add sessions only on demand
         masterIter++;
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
         masterIter++;
         sessionIter++;
      }
   }

   // remaining sessions are removed
   while(sessionIter != sessions.end() )
   {
      NumNodeID currentSession = sessionIter->first;
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
size_t SessionStore::getAllSessionIDs(NumNodeIDList* outSessionIDs)
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

void SessionStore::serializeForTarget(Serializer& ser, uint16_t targetID)
{
   ser % uint32_t(sessions.size());

   for (SessionMapIter it = sessions.begin(), end = sessions.end(); it != end; ++it)
   {
      ser % it->first;
      it->second->getReferencedObject()->serializeForTarget(ser, targetID);
   }

   LOG_DEBUG("SessionStore serialize", Log_DEBUG, "count of serialized Sessions: " +
      StringTk::uintToStr(sessions.size()));
}

void SessionStore::deserializeForTarget(Deserializer& des, uint16_t targetID)
{
   uint32_t elemCount;

   des % elemCount;
   if (unlikely(!des.good()))
      return;

   for(unsigned i = 0; i < elemCount; i++)
   {
      NumNodeID key;

      des % key;
      if (unlikely(!des.good()))
         return;

      Session* session = new Session();
      session->deserializeForTarget(des, targetID);
      if (unlikely(!des.good()))
      {
         session->getLocalFiles()->deleteAllSessions();
         delete(session);
         return;
      }

      SessionMapIter searchResult = this->sessions.find(key);
      if (searchResult == this->sessions.end())
      {
         this->sessions.insert(SessionMapVal(key, new SessionReferencer(session)));
      }
      else
      { // exist so local files will merged
         searchResult->second->getReferencedObject()->mergeSessionLocalFiles(session);
         delete(session);
      }
   }

   LOG_DEBUG("SessionStore deserialize", Log_DEBUG, "count of deserialized Sessions: " +
         StringTk::uintToStr(elemCount));
}

bool SessionStore::loadFromFile(std::string filePath, uint16_t targetID)
{
   LogContext log("SessionStore (load)");
   log.log(Log_DEBUG,"load sessions of target: " + StringTk::uintToStr(targetID));

   bool retVal = false;
   boost::scoped_array<char> buf;
   int readRes;

   struct stat statBuf;
   int retValStat;

   if(!filePath.length() )
      return false;

   SafeMutexLock mutexLock(&mutex);

   int fd = open(filePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      log.log(Log_DEBUG, "Unable to open session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_unlock;
   }

   retValStat = fstat(fd, &statBuf);
   if(retValStat)
   { // stat failed
      log.log(Log_WARNING, "Unable to stat session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_stat;
   }

   buf.reset(new char[statBuf.st_size]);
   readRes = read(fd, buf.get(), statBuf.st_size);
   if(readRes <= 0)
   { // reading failed
      log.log(Log_WARNING, "Unable to read session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   { // parse contents
      Deserializer des(buf.get(), readRes);
      deserializeForTarget(des, targetID);
      retVal = des.good();
   }

   if (retVal == false)
      log.logErr("Could not deserialize SessionStore from file: " + filePath);

err_stat:
   close(fd);

err_unlock:
   mutexLock.unlock();

   return retVal;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool SessionStore::saveToFile(std::string filePath, uint16_t targetID)
{
   LogContext log("SessionStore (save)");
   log.log(Log_DEBUG,"save sessions of target: " + StringTk::uintToStr(targetID));

   bool retVal = false;

   boost::scoped_array<char> buf;
   unsigned bufLen;
   ssize_t writeRes;

   if(!filePath.length() )
      return false;

   SafeMutexLock mutexLock(&mutex); // L O C K

   // create/trunc file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(filePath.c_str(), openFlags, 0666);
   if(fd == -1)
   { // error
      log.logErr("Unable to create session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_unlock;
   }

   // file created => store data
   {
      Serializer lengthCalc;
      serializeForTarget(lengthCalc, targetID);

      bufLen = lengthCalc.size();
      buf.reset(new (std::nothrow) char[bufLen]);
      if (!buf)
      {
         LOG(ERR, "Unable to allocate serializer memory", filePath);
         goto err_closefile;
      }
   }

   {
      Serializer ser(buf.get(), bufLen);
      serializeForTarget(ser, targetID);

      if (!ser.good())
      {
         log.logErr("Unable to serialize session file: " + filePath + ".");
         goto err_closefile;
      }
   }

   writeRes = write(fd, buf.get(), bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      log.logErr("Unable to store session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_closefile;
   }

   retVal = true;

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Session file stored: " + filePath);

   // error compensation
err_closefile:
   close(fd);

err_unlock:
   mutexLock.unlock(); // U N L O C K

   return retVal;
}

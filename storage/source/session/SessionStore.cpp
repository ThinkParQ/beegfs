#include "SessionStore.h"

#include <boost/scoped_array.hpp>

#include <mutex>

/**
 * @return NULL if no such session exists
 */
std::shared_ptr<Session> SessionStore::referenceSession(NumNodeID sessionID) const
{
   std::lock_guard<Mutex> const lock(mutex);

   auto iter = sessions.find(sessionID);
   if (iter != sessions.end())
      return iter->second;

   return nullptr;
}

std::shared_ptr<Session> SessionStore::referenceOrAddSession(NumNodeID sessionID)
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = sessions.find(sessionID);
   if (iter != sessions.end())
      return iter->second;

   // add as new session and reference it
   LogContext log("SessionStore (ref)");
   log.log(Log_DEBUG, std::string("Creating a new session. SessionID: ") + sessionID.str());

   auto session = std::make_shared<Session>(sessionID);
   sessions[sessionID] = session;
   return session;
}

/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @return contained sessions must be cleaned up by the caller
 */
std::list<std::shared_ptr<Session>> SessionStore::syncSessions(
   const std::vector<NodeHandle>& masterList)
{
   std::lock_guard<Mutex> const lock(mutex);

   std::list<std::shared_ptr<Session>> result;

   auto sessionIter = sessions.begin();
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
         auto session = std::move(sessionIter->second);
         sessionIter++; // (removal invalidates iterator)

         result.push_back(std::move(session));
         sessions.erase(std::prev(sessionIter));
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
      auto session = std::move(sessionIter->second);
      sessionIter++; // (removal invalidates iterator)

      result.push_back(std::move(session));
      sessions.erase(std::prev(sessionIter));
   }

   return result;
}

/**
 * @return number of sessions
 */
size_t SessionStore::getAllSessionIDs(NumNodeIDList* outSessionIDs) const
{
   std::lock_guard<Mutex> const lock(mutex);

   size_t retVal = sessions.size();

   for (auto iter = sessions.begin(); iter != sessions.end(); iter++)
      outSessionIDs->push_back(iter->first);

   return retVal;
}

size_t SessionStore::getSize() const
{
   std::lock_guard<Mutex> const lock(mutex);

   return sessions.size();
}

void SessionStore::serializeForTarget(Serializer& ser, uint16_t targetID) const
{
   ser % uint32_t(sessions.size());

   for (auto it = sessions.begin(), end = sessions.end(); it != end; ++it)
   {
      ser % it->first;
      it->second->serializeForTarget(ser, targetID);
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

      auto session = boost::make_unique<Session>();
      session->deserializeForTarget(des, targetID);
      if (unlikely(!des.good()))
      {
         session->getLocalFiles()->deleteAllSessions();
         return;
      }

      auto searchResult = this->sessions.find(key);
      if (searchResult == this->sessions.end())
      {
         this->sessions.insert({key, std::move(session)});
      }
      else
      { // exist so local files will merged
         searchResult->second->mergeSessionLocalFiles(session.get());
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

   std::lock_guard<Mutex> const lock(mutex);

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

   if (!retVal)
      log.logErr("Could not deserialize SessionStore from file: " + filePath);

err_stat:
   close(fd);

err_unlock:
   return retVal;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool SessionStore::saveToFile(std::string filePath, uint16_t targetID) const
{
   LogContext log("SessionStore (save)");
   log.log(Log_DEBUG,"save sessions of target: " + StringTk::uintToStr(targetID));

   bool retVal = false;

   boost::scoped_array<char> buf;
   unsigned bufLen;
   ssize_t writeRes;

   if(!filePath.length() )
      return false;

   std::lock_guard<Mutex> const lock(mutex);

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
         LOG(SESSIONS, ERR, "Unable to allocate serializer memory", filePath);
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
   return retVal;
}

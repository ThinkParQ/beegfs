#include <program/Program.h>
#include "SessionStore.h"

#include <mutex>
#include <boost/scoped_array.hpp>

// these two u32 values are the first eight bytes of a session dump file.
// in 2015.03, the first four bytes were the number of sessions in the dump, so here we use
// 0xffffffff to indicate that we are not using the 2015.03 format - it couldn't store that many
// sessions anyway.
#define SESSION_FORMAT_HEADER  uint32_t(0xffffffffUL)
#define SESSION_FORMAT_VERSION uint32_t(1)

/**
 * @param session belongs to the store after calling this method - so do not free it and don't
 * use it any more afterwards (re-get it from this store if you need it)
 */
void SessionStore::addSession(Session* session)
{
   NumNodeID sessionID = session->getSessionID();

   std::lock_guard<Mutex> mutexLock(mutex);

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
}

/**
 * Note: remember to call releaseSession()
 *
 * @return NULL if no such session exists
 */
Session* SessionStore::referenceSession(NumNodeID sessionID, bool addIfNotExists)
{
   Session* session;

   std::lock_guard<Mutex> mutexLock(mutex);

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

   return session;
}

void SessionStore::releaseSession(Session* session)
{
   NumNodeID sessionID = session->getSessionID();

   std::lock_guard<Mutex> mutexLock(mutex);

   SessionMapIter iter = sessions.find(sessionID);
   if(iter != sessions.end() )
   { // session exists => decrease refCount
      SessionReferencer* sessionRefer = iter->second;
      sessionRefer->release();
   }
}

bool SessionStore::removeSession(NumNodeID sessionID)
{
   bool delErr = true;

   std::lock_guard<Mutex> mutexLock(mutex);

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
 * Removes all sessions from the session store which are not contained in the masterList.
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param outRemovedSessions contained sessions must be cleaned up by the caller
 *                           note: list will NOT be cleared, new elements will be appended
 * @param outUnremovableSesssions contains sessions that would have been removed but are currently
 *                                referenced
 *                                note: list will NOT be cleared, new elements will be appended
 */
void SessionStore::syncSessions(const std::vector<NodeHandle>& masterList,
   SessionList& outRemovedSessions, NumNodeIDList& outUnremovableSessions)
{
   std::lock_guard<Mutex> mutexLock(mutex);

   SessionMapIter sessionIter = sessions.begin();
   auto masterIter = masterList.begin();

   while( (sessionIter != sessions.end() ) && (masterIter != masterList.end() ) )
   {
      NumNodeID currentSession = sessionIter->first;
      NumNodeID currentMaster = (*masterIter)->getNumID() ;

      if(currentMaster < currentSession)
      { // session is added
         // note: we add sessions only on demand, so no operation here
         masterIter++;
      }
      else
      if(currentSession < currentMaster)
      { // session is removed
         sessionIter++; // (removal invalidates iterator)

         Session* session = removeSessionUnlocked(currentSession);
         if(session)
            outRemovedSessions.push_back(session);
         else
            outUnremovableSessions.push_back(currentSession); // session was referenced
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
         outRemovedSessions.push_back(session);
      else
         outUnremovableSessions.push_back(currentSession); // session was referenced
   }
}

/**
 * @return number of sessions
 */
NumNodeIDList SessionStore::getAllSessionIDs()
{
   std::lock_guard<Mutex> mutexLock(mutex);
   NumNodeIDList res;

   for(SessionMapIter iter = sessions.begin(); iter != sessions.end(); iter++)
      res.push_back(iter->first);

   return res;
}

size_t SessionStore::getSize()
{
   std::lock_guard<Mutex> mutexLock(mutex);
   return sessions.size();
}

void SessionStore::serialize(Serializer& ser) const
{
   ser
      % SESSION_FORMAT_HEADER
      % SESSION_FORMAT_VERSION
      % uint32_t(sessions.size());

   for (SessionMap::const_iterator it = sessions.begin(); it != sessions.end(); ++it)
   {
      ser
         % it->first
         % *it->second->getReferencedObject();
   }

   {
      Serializer lenPos = ser.mark();

      ser % uint32_t(0);

      std::set<const FileInode*> inodesSeen;

      for (auto sessionIter = sessions.begin(); sessionIter != sessions.end(); ++sessionIter)
      {
         Session& session = *sessionIter->second->getReferencedObject();
         const auto& fileMap = session.getFiles()->sessions;

         for (auto fileMapIter = fileMap.begin(); fileMapIter != fileMap.end(); ++fileMapIter)
         {
            SessionFile& file = *fileMapIter->second->getReferencedObject();
            const auto& inode = file.getInode();

            if (!inodesSeen.insert(inode.get()).second)
               continue;

            ser
               % *file.getEntryInfo()
               % inode->lockState();
         }
      }

      lenPos % uint32_t(inodesSeen.size());
   }

   LOG_DEBUG("SessionStore serialize", Log_DEBUG, "Serialized session store. Session count: "
         + StringTk::uintToStr(sessions.size()));
}

void SessionStore::deserialize(Deserializer& des)
{
   uint32_t sessionFormatHeader;
   uint32_t sessionFormatVersion;
   uint32_t elemCount;

   des
      % sessionFormatHeader
      % sessionFormatVersion;
   if (sessionFormatHeader != SESSION_FORMAT_HEADER ||
         sessionFormatVersion != SESSION_FORMAT_VERSION)
   {
      des.setBad();
      return;
   }

   des % elemCount;

   // sessions
   for(unsigned i = 0; i < elemCount; i++)
   {
      NumNodeID key;

      des % key;
      if (!des.good())
         return;

      Session* session = new Session();
      des % *session;

      if (!des.good())
      {
         session->getFiles()->deleteAllSessions();
         delete(session);
         return;
      }

      SessionMapIter searchResult = this->sessions.find(key);
      if (searchResult == this->sessions.end() )
      {
         this->sessions.insert(SessionMapVal(key, new SessionReferencer(session)));
      }
      else
      { // exist so local files will merged
         searchResult->second->getReferencedObject()->mergeSessionFiles(session);
         delete(session);
      }
   }

   LOG_DEBUG("SessionStore deserialize", Log_DEBUG, "Deserialized session store. Session count: "
         + StringTk::uintToStr(elemCount));
}

void SessionStore::deserializeLockStates(Deserializer& des, MetaStore& metaStore)
{
   uint32_t elemCount;

   des % elemCount;

   // file inode lock states
   while (elemCount > 0)
   {
      elemCount--;

      EntryInfo info;

      des % info;

      if (!des.good())
         break;

      auto inode = metaStore.referenceFile(&info);

      des % inode->lockState();

      metaStore.releaseFile(info.getParentEntryID(), inode);

      if (!des.good())
         break;
   }

   if (!des.good())
   {
      LOG(SESSIONS, ERR, "Could not restore file inode lock states.");
      return;
   }
}

/**
 * Note: Caller must either hold a lock on the SessionStore, or otherwise ensure that it is not
 *       accessed from anywhere.
 */
bool SessionStore::deserializeFromBuf(const char* buf, size_t bufLen, MetaStore& metaStore)
{
   Deserializer des(buf, bufLen);
   deserialize(des);

   if (!des.good())
      LOG(SESSIONS, ERR, "Unable to deserialize session store from buffer.");

   const bool relinkRes = relinkInodes(metaStore);
   if (!relinkRes)
      LOG(SESSIONS, ERR, "Unable to relink inodes.");

   if (!des.good() || !relinkRes)
      return false;

   deserializeLockStates(des, metaStore);
   if (!des.good())
   {
      LOG(SESSIONS, ERR, "Unable to load lock states.");
      return false;
   }

   return true;
}

bool SessionStore::loadFromFile(const std::string& filePath, MetaStore& metaStore)
{
   LogContext log("SessionStore (load)");
   log.log(Log_DEBUG,"load sessions from file: " + filePath);

   bool retVal = false;
   boost::scoped_array<char> buf;
   int readRes;

   struct stat statBuf;
   int retValStat;

   if(filePath.empty())
      return false;

   std::lock_guard<Mutex> mutexLock(mutex);

   int fd = open(filePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      log.log(Log_DEBUG, "Unable to open session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   retValStat = fstat(fd, &statBuf);
   if(retValStat)
   { // stat failed
      log.log(Log_WARNING, "Unable to stat session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_stat;
   }

   if (statBuf.st_size == 0)
   {
      LOG(SESSIONS, WARNING, "Session file exists, but is empty.", filePath);
      return false;
   }

   buf.reset(new (std::nothrow) char[statBuf.st_size]);
   if (!buf)
   {
      LOG(SESSIONS, ERR, "Could not allocate memory to read session file", filePath);
      goto err_stat;
   }

   readRes = read(fd, buf.get(), statBuf.st_size);
   if(readRes <= 0)
   { // reading failed
      log.log(Log_WARNING, "Unable to read session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   { // parse contents
      retVal = deserializeFromBuf(buf.get(), readRes, metaStore);
   }

   if (!retVal)
      log.logErr("Could not deserialize SessionStore from file: " + filePath);

err_stat:
   close(fd);

   return retVal;
}

/*
 * @returns pair of buffer and buffer size, { nullptr, 0 } on error.
 */
std::pair<std::unique_ptr<char[]>, size_t> SessionStore::serializeToBuf()
{
   size_t bufLen;
   std::unique_ptr<char[]> buf;

   {
      Serializer ser;
      serialize(ser);

      bufLen = ser.size();
      buf.reset(new (std::nothrow)char[bufLen]);

      if (!buf)
      {
         LOG(SESSIONS, ERR, "Could not allocate memory to serialize sessions.");
         return { std::unique_ptr<char[]>(), 0 };
      }
   }

   {
      Serializer ser(buf.get(), bufLen);
      serialize(ser);
      if (!ser.good())
      {
         LOG(SESSIONS, ERR, "Unable to serialize sessions.");
         return { std::unique_ptr<char[]>(), 0 };
      }
   }

   return { std::move(buf), bufLen };
}

/**
 * Note: setStorePath must be called before using this.
 */
bool SessionStore::saveToFile(const std::string& filePath)
{
   LogContext log("SessionStore (save)");
   log.log(Log_DEBUG,"save sessions to file: " + filePath);

   bool retVal = false;

   if(!filePath.length() )
      return false;

   std::lock_guard<Mutex> mutexLock(mutex);

   // create/trunc file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(filePath.c_str(), openFlags, 0666);
   if(fd == -1)
   { // error
      log.logErr("Unable to create session file: " + filePath + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   // file created => store data
   {
      ssize_t writeRes;

      auto buf = serializeToBuf();
      if (!buf.first)
      {
         LOG(SESSIONS, ERR, "Unable to save session file", filePath);
         goto err_closefile;
      }

      writeRes = write(fd, buf.first.get(), buf.second);

      if(writeRes != (ssize_t)buf.second)
      {
         log.logErr("Unable to store session file: " + filePath + ". " +
            "SysErr: " + System::getErrString() );

         goto err_closefile;
      }
   }

   retVal = true;

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Session file stored: " + filePath);

   // error compensation
err_closefile:
   close(fd);

   return retVal;
}

/**
 * Clears out all sessions from session store.
 * Caller must hold lock on session store or otherwise ensure no access to the session store takes
 * place.
 * Intended for using prior to a session store resync on the secondary of a mirror buddy group.
 * This method *does not* unlink disposed files or remove their chunk files on storage servers,
 * since this may invalidate results of previous resync activity or conflict with the role of the
 * current server as secondary in a mirror group.
 * @returns true if all sessions could be removed, false if there are sessions still referenced and
 *          can't be removed.
 */
bool SessionStore::clear()
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   for (SessionMapIter sessionIt = sessions.begin(); sessionIt != sessions.end(); /* inc in body */)
   {
      // Iter is incremented here because removeSessionUnlocked invalidates it.
      Session* session = removeSessionUnlocked((sessionIt++)->first);
      if (session)
      {
         // session was removed - clean it up
         SessionFileStore* sessionFiles = session->getFiles();

         SessionFileList removedSessionFiles;
         UIntList referencedSessionFiles;

         sessionFiles->removeAllSessions(&removedSessionFiles, &referencedSessionFiles);

         // referencedSessionFiles should always be empty, because otherwise, the session itself
         // would also still be referenced somewhere, so we wouldn't end up here. Nevertheless,
         // check + log.
         if (!referencedSessionFiles.empty())
         {
            LOG(SESSIONS, ERR, "Session file still references in unreferenced session.");
            continue;
         }

         for (SessionFileListIter fileIt = removedSessionFiles.begin();
              fileIt != removedSessionFiles.end(); ++fileIt)
         {
            SessionFile* sessionFile = *fileIt;
            unsigned accessFlags = sessionFile->getAccessFlags();
            unsigned numHardlinks;
            unsigned numInodeRefs;

            MetaFileHandle inode = sessionFile->releaseInode();

            EntryInfo* entryInfo = sessionFile->getEntryInfo();
            metaStore->closeFile(entryInfo, std::move(inode), accessFlags, &numHardlinks,
                  &numInodeRefs);

            delete sessionFile;
         }

         delete session;
      }
      else
      {
         LOG(SESSIONS, ERR, "Session still referenced", ("ID", sessionIt->first));
      }
   }

   return true;
}

bool SessionStore::operator==(const SessionStore& other) const
{
   struct ops {
      static bool cmp(const SessionMapVal& lhs, const SessionMapVal& rhs)
      {
         return lhs.first == rhs.first
            && *lhs.second->getReferencedObject() == *rhs.second->getReferencedObject();
      }
   };

   return sessions.size() == other.sessions.size()
      && std::equal(
            sessions.begin(), sessions.end(),
            other.sessions.begin(),
            ops::cmp);
}

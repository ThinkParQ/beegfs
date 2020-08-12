#include <app/App.h>
#include <program/Program.h>
#include "SessionLocalFileStore.h"


/**
 * Add a new session to the store (if it doesn't exist yet) and return a referenced version
 * of the session with this ID.
 *
 * @param session belongs to the store after calling this method
 */
std::shared_ptr<SessionLocalFile> SessionLocalFileStore::addAndReferenceSession(
   std::unique_ptr<SessionLocalFile> session)
{
   std::string fileHandleID(session->getFileHandleID() );
   uint16_t targetID = session->getTargetID();
   bool isMirrorSession = session->getIsMirrorSession();

   std::lock_guard<Mutex> const lock(mutex);

   // try to insert the new session
   auto insertRes = sessions.insert(
         {Key{fileHandleID, targetID, isMirrorSession}, std::move(session)});

   // reference session (note: insertRes.first is an iterator to the inserted/existing session)
   return insertRes.first->second;
}

/**
 * @param targetID really targetID (not buddy group ID for mirrors, because both buddies can be
 *        attached to the same server).
 * @param isMirrorSession true if this is a session for a mirrored chunk file.
 * @return NULL if no such session exists.
 */
std::shared_ptr<SessionLocalFile> SessionLocalFileStore::referenceSession(
   const std::string& fileHandleID, uint16_t targetID, bool isMirrorSession) const
{
   std::lock_guard<Mutex> const lock(mutex);

   auto iter = sessions.find({fileHandleID, targetID, isMirrorSession});
   if (iter != sessions.end())
      return iter->second;

   return nullptr;
}

/**
 * @param isMirrorSession true if this is a session for a mirror chunk file
 * @return filesystem state of the session if it was unused, nullptr otherwise.
 */
std::shared_ptr<SessionLocalFile::Handle> SessionLocalFileStore::removeSession(
   const std::string& fileHandleID, uint16_t targetID, bool isMirrorSession)
{
   std::shared_ptr<SessionLocalFile> file;

   {
      std::lock_guard<Mutex> const lock(mutex);

      auto iter = sessions.find({fileHandleID, targetID, isMirrorSession});
      if (iter == sessions.end())
         return nullptr;

      file = std::move(iter->second);
      sessions.erase(iter);
   }

   return releaseLastReference(std::move(file));
}

size_t SessionLocalFileStore::removeAllSessions()
{
   std::lock_guard<Mutex> const lock(mutex);

   size_t total = sessions.size();

   sessions.clear();

   return total;
}

/**
 * Removes all mirror sessions on a specific target.
 */
void SessionLocalFileStore::removeAllMirrorSessions(uint16_t targetID)
{
   std::lock_guard<Mutex> const lock(mutex);

   for (auto iter = sessions.begin(); iter != sessions.end(); )
   {
      auto& session = iter->second;
      ++iter;

      if (session->getTargetID() == targetID && session->getIsMirrorSession())
         sessions.erase(std::prev(iter));
   }
}

/*
 * Intended to be used for cleanup if deserialization failed, no locking is used
 */
void SessionLocalFileStore::deleteAllSessions()
{
   sessions.clear();
}

size_t SessionLocalFileStore::getSize() const
{
   std::lock_guard<Mutex> const lock(mutex);

   return sessions.size();
}

/* Merges the SessionLocalFiles of the given SessionLocalFileStore into this SessionLocalFileStore.
 * Only not existing SessionLocalFiles will be added to the existing SessionLocalFileStore
 *
 * @param sessionLocalFileStore the sessionLocalFileStore which will be merged with this
 * sessionLocalFileStore
 */
void SessionLocalFileStore::mergeSessionLocalFiles(SessionLocalFileStore* sessionLocalFileStore)
{
   for (auto sessionIter = sessionLocalFileStore->sessions.begin();
         sessionIter != sessionLocalFileStore->sessions.end();
         ++sessionIter)
   {
      if (sessions.count(sessionIter->first))
      {
         const auto& id = sessionIter->first;

         LOG(GENERAL, WARNING, "Found SessionLocalFile with same ID, merge not possible, may be a bug?",
               id.fileHandleID, id.targetID, id.isMirrored);
         continue;
      }

      sessions[sessionIter->first] = std::move(sessionIter->second);
   }
}

void SessionLocalFileStore::serializeForTarget(Serializer& ser, uint16_t targetID)
{
   Serializer atStart = ser.mark();

   uint32_t elemCount = 0;
   ser % elemCount; // needs fixup

   for (auto it = sessions.begin(), end = sessions.end(); it != end; ++it)
   {
      if (it->first.targetID != targetID)
         continue;

      ser
         % it->first
         % *it->second;
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
      Key key;
      auto sessionLocalFile = boost::make_unique<SessionLocalFile>();

      des
         % key
         % *sessionLocalFile;

      if (unlikely(!des.good() || key.targetID != targetID))
      {
         des.setBad();
         return;
      }

      this->sessions.insert({key, std::move(sessionLocalFile)});
   }

   LOG_DEBUG("SessionLocalFileStore deserialize", Log_DEBUG, "count of deserialized "
      "SessionLocalFiles: " + StringTk::uintToStr(elemCount));
}

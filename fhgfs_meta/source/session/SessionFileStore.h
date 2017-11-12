#ifndef SESSIONFILESTORE_H_
#define SESSIONFILESTORE_H_

#include <common/toolkit/ObjectReferencer.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/Random.h>
#include <common/Common.h>
#include "SessionFile.h"

typedef ObjectReferencer<SessionFile*> SessionFileReferencer;
typedef std::map<uint32_t, SessionFileReferencer*> SessionFileMap;
typedef SessionFileMap::iterator SessionFileMapIter;
typedef SessionFileMap::const_iterator SessionFileMapCIter;
typedef SessionFileMap::value_type SessionFileMapVal;

typedef std::list<SessionFile*> SessionFileList;
typedef SessionFileList::iterator SessionFileListIter;

class SessionStore;

class SessionFileStore
{
   friend class SessionStore;

   public:
      SessionFileStore()
      {
         // note: randomize to make it more unlikely that after a meta server shutdown the same
         // client instance can open a file and gets a colliding (with the old meta server
         // instance) sessionID
         this->lastSessionID = Random().getNextInt();
      }

      unsigned addSession(SessionFile* session);
      bool addSession(SessionFile* session, unsigned sessionFileID);
      SessionFile* addAndReferenceRecoverySession(SessionFile* session);
      SessionFile* referenceSession(unsigned sessionID);
      void releaseSession(SessionFile* session, EntryInfo* entryInfo);
      bool removeSession(unsigned sessionID);
      void removeAllSessions(SessionFileList* outRemovedSessions,
         UIntList* outReferencedSessions);
      void deleteAllSessions();
      void mergeSessionFiles(SessionFileStore* sessionFileStore);

      size_t getSize();

      bool relinkInodes(MetaStore& store)
      {
         bool result = true;

         for (auto it = sessions.begin(); it != sessions.end(); )
         {
            const bool relinkRes = it->second->getReferencedObject()->relinkInode(store);

            if (!relinkRes)
               sessions.erase(it++);
            else
               ++it;

            result &= relinkRes;
         }

         return result;
      }

      static void serialize(const SessionFileStore* obj, Serializer& ser)
      {
         ser
            % obj->lastSessionID
            % uint32_t(obj->sessions.size());

         for (SessionFileMapCIter it = obj->sessions.begin(); it != obj->sessions.end(); ++it)
         {
            ser
               % it->first
               % *it->second->getReferencedObject();
         }

         LOG_DEBUG("SessionFileStore serialize", Log_DEBUG, "count of serialized "
            "SessionFiles: " + StringTk::uintToStr(obj->sessions.size()) );
      }

      static void serialize(SessionFileStore* obj, Deserializer& des)
      {
         uint32_t elemCount;

         des
            % obj->lastSessionID
            % elemCount;

         for(unsigned i = 0; i < elemCount; i++)
         {
            uint32_t key;

            des % key;
            if (!des.good())
               return;

            SessionFile* sessionFile = new SessionFile();
            des % *sessionFile;
            if (!des.good())
            {
               delete(sessionFile);
               return;
            }

            obj->sessions.insert(SessionFileMapVal(key, new SessionFileReferencer(sessionFile)));
         }

         LOG_DEBUG("SessionFileStore deserialize", Log_DEBUG, "count of deserialized "
            "SessionFiles: " + StringTk::uintToStr(elemCount));
      }

      bool operator==(const SessionFileStore& other) const;

      bool operator!=(const SessionFileStore& other) const { return !(*this == other); }

   protected:
      SessionFileMap* getSessionMap();

   private:
      SessionFileMap sessions;
      uint32_t lastSessionID;

      Mutex mutex;

      SessionFile* removeAndGetSession(unsigned sessionID); // actually not needed now

      unsigned generateNewSessionID();

      void performAsyncCleanup(EntryInfo* entryInfo, MetaFileHandle inode, unsigned accessFlags);
};

#endif /*SESSIONFILESTORE_H_*/

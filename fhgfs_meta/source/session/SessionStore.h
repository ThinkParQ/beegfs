#ifndef META_SESSIONSTORE_H
#define META_SESSIONSTORE_H

#include <common/nodes/Node.h>
#include <common/toolkit/ObjectReferencer.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
#include <session/EntryLockStore.h>

#include "Session.h"


typedef ObjectReferencer<Session*> SessionReferencer;
typedef std::map<NumNodeID, SessionReferencer*> SessionMap;
typedef SessionMap::iterator SessionMapIter;
typedef SessionMap::value_type SessionMapVal;

typedef std::list<Session*> SessionList;
typedef SessionList::iterator SessionListIter;

/*
 * A session always belongs to a client ID, therefore the session ID is always the nodeID of the
 * corresponding client
 */
class SessionStore
{
      friend class TestSerialization; // for testing

   public:
      SessionStore() {}

      Session* referenceSession(NumNodeID sessionID, bool addIfNotExists=true);
      void releaseSession(Session* session);
      void syncSessions(const std::vector<NodeHandle>& masterList, SessionList& outRemovedSessions,
         NumNodeIDList& outUnremovableSessions);

      NumNodeIDList getAllSessionIDs();
      size_t getSize();

      void serialize(Serializer& ser) const;
      void deserialize(Deserializer& des);
      void deserializeLockStates(Deserializer& des, MetaStore& metaStore);

      std::pair<std::unique_ptr<char[]>, size_t> serializeToBuf();
      bool deserializeFromBuf(const char* buf, size_t bufLen, MetaStore& metaStore);
      bool loadFromFile(const std::string& filePath, MetaStore& metaStore);
      bool saveToFile(const std::string& filePath);

      bool clear();

      EntryLockStore* getEntryLockStore()
      {
         return &this->entryLockStore;
      }

      bool operator==(const SessionStore& other) const;

      bool operator!=(const SessionStore& other) const { return !(*this == other); }

   private:
      SessionMap sessions;

      Mutex mutex;

      EntryLockStore entryLockStore; // Locks on entryIDs to synchronize with mirror buddy.

      void addSession(Session* session); // actually not needed (maybe later one day...)
      bool removeSession(NumNodeID sessionID); // actually not needed (maybe later one day...)
      Session* removeSessionUnlocked(NumNodeID sessionID);

      bool relinkInodes(MetaStore& store)
      {
         bool result = true;

         for (auto it = sessions.begin(); it != sessions.end(); )
         {
            const bool relinkRes = it->second->getReferencedObject()->relinkInodes(store);

            if (!relinkRes)
            {
               // Relinking failed on at least one SessionFile in this Session -> remove if empty
               if (it->second->getReferencedObject()->getFiles()->getSize() == 0)
                  sessions.erase(it++);
               else
                  ++it;
            }
            else
            {
               ++it;
            }

            result &= relinkRes;
         }

         return result;
      }
};

#endif /* META_SESSIONSTORE_H */

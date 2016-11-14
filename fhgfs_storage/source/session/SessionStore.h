#ifndef SESSIONSTORE_H_
#define SESSIONSTORE_H_

#include <common/nodes/Node.h>
#include <common/toolkit/ObjectReferencer.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
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
   public:
      SessionStore() {}

      Session* referenceSession(NumNodeID sessionID, bool addIfNotExists=true);
      void releaseSession(Session* session);
      void syncSessions(const std::vector<NodeHandle>& masterList, SessionList* outRemovedSessions,
         NumNodeIDList* outUnremovableSesssions);

      size_t getAllSessionIDs(NumNodeIDList* outSessionIDs);
      size_t getSize();

      void serializeForTarget(Serializer& ser, uint16_t targetID);
      void deserializeForTarget(Deserializer& des, uint16_t targetID);

      bool loadFromFile(std::string filePath, uint16_t targetID);
      bool saveToFile(std::string filePath, uint16_t targetID);

   private:
      SessionMap sessions;

      Mutex mutex;

      void addSession(Session* session); // actually not needed (maybe later one day...)
      bool removeSession(NumNodeID sessionID); // actually not needed (maybe later one day...)
      Session* removeSessionUnlocked(NumNodeID sessionID);
};

#endif /*SESSIONSTORE_H_*/

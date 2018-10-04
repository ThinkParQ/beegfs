#ifndef SESSIONSTORE_H_
#define SESSIONSTORE_H_

#include <common/nodes/Node.h>
#include <common/toolkit/ObjectReferencer.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
#include "Session.h"

/*
 * A session always belongs to a client ID, therefore the session ID is always the nodeID of the
 * corresponding client
 */
class SessionStore
{
   public:
      SessionStore() {}

      std::shared_ptr<Session> referenceSession(NumNodeID sessionID) const;
      std::shared_ptr<Session> referenceOrAddSession(NumNodeID sessionID);
      std::list<std::shared_ptr<Session>> syncSessions(const std::vector<NodeHandle>& masterList);

      size_t getAllSessionIDs(NumNodeIDList* outSessionIDs) const;
      size_t getSize() const;

      void serializeForTarget(Serializer& ser, uint16_t targetID) const;
      void deserializeForTarget(Deserializer& des, uint16_t targetID);

      bool loadFromFile(std::string filePath, uint16_t targetID);
      bool saveToFile(std::string filePath, uint16_t targetID) const;

   private:
      std::map<NumNodeID, std::shared_ptr<Session>> sessions;

      mutable Mutex mutex;
};

#endif /*SESSIONSTORE_H_*/

#ifndef SESSION_H_
#define SESSION_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include "SessionLocalFileStore.h"

/*
 * A session always belongs to a client ID, therefore the session ID is always the nodeID of the
 * corresponding client
 */
class Session
{
   public:
      Session(const NumNodeID sessionID) : sessionID(sessionID) {}

      /*
       * For deserialization only
       */
      Session() {};

      void mergeSessionLocalFiles(Session* session);

      void serializeForTarget(Serializer& ser, uint16_t targetID);
      void deserializeForTarget(Deserializer& des, uint16_t targetID);

   private:
      NumNodeID sessionID;

      SessionLocalFileStore localFiles;

   public:
      // getters & setters
      NumNodeID getSessionID() const
      {
         return sessionID;
      }

      SessionLocalFileStore* getLocalFiles()
      {
         return &localFiles;
      }
};


#endif /*SESSION_H_*/

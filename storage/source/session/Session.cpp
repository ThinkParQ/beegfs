#include <common/toolkit/serialization/Serialization.h>
#include "Session.h"


/* Merges the SessionLocalFiles of the given session into this session, only not existing
 * SessionLocalFiles will be added to the existing session
 * @param session the session which will be merged with this session
 */
void Session::mergeSessionLocalFiles(Session* session)
{
   this->getLocalFiles()->mergeSessionLocalFiles(session->getLocalFiles());
}

void Session::serializeForTarget(Serializer& ser, uint16_t targetID)
{
   ser % sessionID;
   localFiles.serializeForTarget(ser, targetID);
}

void Session::deserializeForTarget(Deserializer& des, uint16_t targetID)
{
   des % sessionID;
   localFiles.deserializeForTarget(des, targetID);
}

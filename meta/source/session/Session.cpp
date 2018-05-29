#include "Session.h"

/* Merges the SessionFiles of the given session into this session, only not existing
 * SessionFiles will be added to the existing session
 * @param session the session which will be merged with this session
 */
void Session::mergeSessionFiles(Session* session)
{
   this->getFiles()->mergeSessionFiles(session->getFiles() );
}

bool Session::operator==(const Session& other) const
{
   return sessionID == other.sessionID
      && files == other.files;
}

#include "SessionStoreResyncer.h"

#include <common/toolkit/MessagingTk.h>
#include <common/net/message/storage/mirroring/ResyncSessionStoreMsg.h>
#include <common/net/message/storage/mirroring/ResyncSessionStoreRespMsg.h>
#include <common/toolkit/StringTk.h>
#include <program/Program.h>
#include <app/App.h>

#include <boost/scoped_array.hpp>

SessionStoreResyncer::SessionStoreResyncer(const NumNodeID& buddyNodeID)
 : buddyNodeID(buddyNodeID) {}

void SessionStoreResyncer::doSync()
{
   App* app = Program::getApp();
   SessionStore* sessions = app->getMirroredSessions();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   const uint64_t numSessions = sessions->getSize();

   numSessionsToSync.set(numSessions);

   // Serialize sessions store into buffer
   std::pair<std::unique_ptr<char[]>, size_t> sessionStoreSerBuf = sessions->serializeToBuf();

   if (sessionStoreSerBuf.second == 0)
   {
      // Serialization failed.
      errors.set(1);
      return;
   }

   LOG(MIRRORING, DEBUG, "Serialized session store", ("size", sessionStoreSerBuf.second));

   ResyncSessionStoreMsg msg(sessionStoreSerBuf.first.get(), sessionStoreSerBuf.second);
   RequestResponseArgs rrArgs(NULL, &msg, NETMSGTYPE_ResyncSessionStoreResp);
   RequestResponseNode rrNode(buddyNodeID, metaNodes);
   msg.registerStreamoutHook(rrArgs);

   FhgfsOpsErr requestRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

   if (requestRes != FhgfsOpsErr_SUCCESS)
   {
      errors.set(1);
      LOG(MIRRORING, ERR, "Request failed.", requestRes);
      return;
   }

   ResyncSessionStoreRespMsg* resp = (ResyncSessionStoreRespMsg*)rrArgs.outRespMsg.get();
   FhgfsOpsErr retVal = resp->getResult();

   LOG(MIRRORING, DEBUG, "ResyncSessionStoreRespMsg", retVal);

   if (retVal != FhgfsOpsErr_SUCCESS)
      errors.set(1);
   else
      numSessionsSynced.set(numSessions);
}

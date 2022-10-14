#include "ResyncSessionStoreMsgEx.h"

#include <app/App.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include <common/net/message/storage/mirroring/ResyncSessionStoreRespMsg.h>

bool ResyncSessionStoreMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Config* config = app->getConfig();

   FhgfsOpsErr receiveRes = receiveStoreBuf(ctx.getSocket(), config->getConnMsgShortTimeout());

   if (receiveRes == FhgfsOpsErr_OUTOFMEM)
   {
      LOG(MIRRORING, ERR, "Failed to allocate receive buffer for session store resync - out of memory.");
      return false;
   }
   else if (receiveRes == FhgfsOpsErr_COMMUNICATION)
   {
      LOG(MIRRORING, ERR, "Failed to receive session store buffer during resync.");
      return false;
   }

   auto sessionStoreBuf = getSessionStoreBuf();

   SessionStore* sessionStore = Program::getApp()->getMirroredSessions();

   const bool clearRes = sessionStore->clear();
   if (!clearRes)
   {
      LOG(MIRRORING, ERR, "Failed to clear session store.");
      ctx.sendResponse(ResyncSessionStoreRespMsg(FhgfsOpsErr_INTERNAL));
      return true;
   }

   const bool deserRes = sessionStore->deserializeFromBuf(sessionStoreBuf.first,
         sessionStoreBuf.second, *Program::getApp()->getMetaStore());
   if (!deserRes)
   {
      LOG(MIRRORING, ERR, "Failed to deserialize session store data from primary.");
      ctx.sendResponse(ResyncSessionStoreRespMsg(FhgfsOpsErr_INTERNAL));
      return true;
   }

   ctx.sendResponse(ResyncSessionStoreRespMsg(FhgfsOpsErr_SUCCESS));
   return true;
}

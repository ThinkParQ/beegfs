#include "RefreshStoragePoolsMsgEx.h"

#include <program/Program.h>

bool RefreshStoragePoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(STORAGEPOOLS, DEBUG, "Received a RefreshStoragePoolsMsg", ctx.peerName());

   Program::getApp()->getInternodeSyncer()->setForceStoragePoolsUpdate();

   // can only come as an AcknowledgableMsg from mgmtd
   acknowledge(ctx);

   return true;
}


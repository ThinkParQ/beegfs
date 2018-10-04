#include "RefreshStoragePoolsMsgEx.h"

#include <program/Program.h>

bool RefreshStoragePoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   Program::getApp()->getInternodeSyncer()->setForceStoragePoolsUpdate();

   // can only come as an AcknowledgableMsg from mgmtd
   acknowledge(ctx);

   return true;
}


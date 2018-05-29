// messages for DEEP-ER cache
#include <common/cache/net/message/flush/FlushCrcRespMsg.h>
#include <common/cache/net/message/flush/FlushIsFinishedRespMsg.h>
#include <common/cache/net/message/flush/FlushRangeRespMsg.h>
#include <common/cache/net/message/flush/FlushRespMsg.h>
#include <common/cache/net/message/flush/FlushStopRespMsg.h>
#include <common/cache/net/message/flush/FlushWaitRespMsg.h>
#include <common/cache/net/message/flush/FlushWaitCrcRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchCrcRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchIsFinishedRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchRangeRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchStopRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchWaitCrcRespMsg.h>
#include <common/cache/net/message/prefetch/PrefetchWaitRespMsg.h>

// control messages
#include <common/net/message/control/AuthenticateChannelMsgEx.h>
#include <common/net/message/control/GenericResponseMsg.h>

#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleMsg.h>
#include "NetMessageFactory.h"



/**
 * @return NetMessage that must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* NetMessageFactory::createFromMsgType(unsigned short msgType)
{
   NetMessage* msg;

   switch(msgType)
   {
      // control messages
      case NETMSGTYPE_AuthenticateChannel: { msg = new AuthenticateChannelMsgEx(); } break;
      case NETMSGTYPE_GenericResponse: { msg = new GenericResponseMsg(); } break;

      // messages for DEEP-ER cache
      case NETMSGTYPE_CachePrefetchResp: { msg = new PrefetchRespMsg(); } break;
      case NETMSGTYPE_CachePrefetchRangeResp: { msg = new PrefetchRangeRespMsg(); } break;
      case NETMSGTYPE_CachePrefetchWaitResp: { msg = new PrefetchWaitRespMsg(); } break;
      case NETMSGTYPE_CachePrefetchCrcResp: { msg = new PrefetchCrcRespMsg(); } break;
      case NETMSGTYPE_CachePrefetchCrcWaitResp: { msg = new PrefetchWaitCrcRespMsg(); } break;
      case NETMSGTYPE_CachePrefetchIsFinishedResp: { msg = new PrefetchIsFinishedRespMsg(); } break;
      case NETMSGTYPE_CachePrefetchStopResp: { msg = new PrefetchStopRespMsg(); } break;
      case NETMSGTYPE_CacheFlushResp: { msg = new FlushRespMsg(); } break;
      case NETMSGTYPE_CacheFlushRangeResp: { msg = new FlushRangeRespMsg(); } break;
      case NETMSGTYPE_CacheFlushWaitResp: { msg = new FlushWaitRespMsg(); } break;
      case NETMSGTYPE_CacheFlushCrcResp: { msg = new FlushCrcRespMsg(); } break;
      case NETMSGTYPE_CacheFlushCrcWaitResp: { msg = new FlushWaitCrcRespMsg(); } break;
      case NETMSGTYPE_CacheFlushIsFinishedResp: { msg = new FlushIsFinishedRespMsg(); } break;
      case NETMSGTYPE_CacheFlushStopResp: { msg = new FlushStopRespMsg(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return msg;
}


// messages for DEEP-ER cache
#include <net/message/flush/FlushCrcMsgEx.h>
#include <net/message/flush/FlushIsFinishedMsgEx.h>
#include <net/message/flush/FlushMsgEx.h>
#include <net/message/flush/FlushRangeMsgEx.h>
#include <net/message/flush/FlushStopMsgEx.h>
#include <net/message/flush/FlushWaitMsgEx.h>
#include <net/message/flush/FlushWaitCrcMsgEx.h>
#include <net/message/prefetch/PrefetchCrcMsgEx.h>
#include <net/message/prefetch/PrefetchIsFinishedMsgEx.h>
#include <net/message/prefetch/PrefetchMsgEx.h>
#include <net/message/prefetch/PrefetchRangeMsgEx.h>
#include <net/message/prefetch/PrefetchStopMsgEx.h>
#include <net/message/prefetch/PrefetchWaitCrcMsgEx.h>
#include <net/message/prefetch/PrefetchWaitMsgEx.h>

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
      case NETMSGTYPE_CachePrefetch: { msg = new PrefetchMsgEx(); } break;
      case NETMSGTYPE_CachePrefetchRange: { msg = new PrefetchRangeMsgEx(); } break;
      case NETMSGTYPE_CachePrefetchWait: { msg = new PrefetchWaitMsgEx(); } break;
      case NETMSGTYPE_CachePrefetchCrc: { msg = new PrefetchCrcMsgEx(); } break;
      case NETMSGTYPE_CachePrefetchCrcWait: { msg = new PrefetchWaitCrcMsgEx(); } break;
      case NETMSGTYPE_CachePrefetchIsFinished: { msg = new PrefetchIsFinishedMsgEx(); } break;
      case NETMSGTYPE_CachePrefetchStop: { msg = new PrefetchStopMsgEx(); } break;
      case NETMSGTYPE_CacheFlush: { msg = new FlushMsgEx(); } break;
      case NETMSGTYPE_CacheFlushRange: { msg = new FlushRangeMsgEx(); } break;
      case NETMSGTYPE_CacheFlushWait: { msg = new FlushWaitMsgEx(); } break;
      case NETMSGTYPE_CacheFlushCrc: { msg = new FlushCrcMsgEx(); } break;
      case NETMSGTYPE_CacheFlushCrcWait: { msg = new FlushWaitCrcMsgEx(); } break;
      case NETMSGTYPE_CacheFlushIsFinished: { msg = new FlushIsFinishedMsgEx(); } break;
      case NETMSGTYPE_CacheFlushStop: { msg = new FlushStopMsgEx(); } break;

      default:
      {
         msg = new SimpleMsg(NETMSGTYPE_Invalid);
      } break;
   }

   return msg;
}


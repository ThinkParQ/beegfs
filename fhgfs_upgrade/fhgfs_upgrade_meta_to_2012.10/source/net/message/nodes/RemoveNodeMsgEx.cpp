#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RemoveNodeMsgEx.h"


bool RemoveNodeMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("RemoveNodeMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   LOG_DEBUG_CONTEXT(log, Log_DEBUG, std::string("Received a RemoveNodeMsg from: ") + peer);
   
   LOG_DEBUG_CONTEXT(log, Log_SPAM, std::string("NodeID of removed node: ") +
      getNodeID() + " / " + StringTk::uintToStr(getNodeNumID() ) );

   App* app = Program::getApp();
   bool logNodeRemoved = false;
   bool logNumNodes = false;
   
   switch(getNodeType() )
   {
      case NODETYPE_Meta:
      case NODETYPE_Storage:
      {
         NodeStoreServersEx* nodes = app->getServerStoreFromType(getNodeType() );
         bool delRes = nodes->deleteNode(getNodeNumID() );
         if(delRes)
         {
            logNodeRemoved = true;
            logNumNodes = true;
         }
      } break;

      case NODETYPE_Client:
      {
         /* currently ignored, because removed clients/sessions are handled by internodesyner
            asynchronously. */
      } break;
      
      default: break;
   }
   
   // log
   
   if(logNodeRemoved)
   {
      log.log(Log_WARNING, "Node removed: " + StringTk::uintToStr(getNodeNumID() ) +
         " (Type: " + Node::nodeTypeToStr( (NodeType)getNodeType() ) + ")" );
   }
   
   if(logNumNodes)
   {
      log.log(Log_WARNING, std::string("Number of nodes in the system: ") +
         "Meta: " + StringTk::intToStr(app->getMetaNodes()->getSize() ) + "; "
         "Storage: " + StringTk::intToStr(app->getStorageNodes()->getSize() ) );
   }

   // send response

   if(!MsgHelperAck::respondToAckRequest(this, fromAddr, sock,
      respBuf, bufLen, app->getDatagramListener() ) )
   {
      RemoveNodeRespMsg respMsg(0);
      respMsg.serialize(respBuf, bufLen);

      if(fromAddr)
      { // datagram => sync via dgramLis send method
         app->getDatagramListener()->sendto(respBuf, respMsg.getMsgLength(), 0,
            (struct sockaddr*)fromAddr, sizeof(*fromAddr) );
      }
      else
         sock->sendto(respBuf, respMsg.getMsgLength(), 0, NULL, 0);
   }

   return true;
}


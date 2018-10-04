#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/NetMessage.h>
#include "IncomingDataWork.h"


void IncomingDataWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   const char* logContextStr = "Work (process incoming data)";

   const int recvTimeoutMS = 5000;

   unsigned numReceived = 0;

   sock->setStats(&stats);

   try
   {
      // receive at least the message header

      numReceived += sock->recvExactT(bufIn, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);

      unsigned msgLength = NetMessageHeader::extractMsgLengthFromBuf(bufIn, numReceived);

      if(unlikely(msgLength > bufInLen) )
      { // message too big
         LogContext(logContextStr).log(Log_NOTICE,
            std::string("Received a message that is too large. Disconnecting: ") +
            sock->getPeername() );

         sock->unsetStats();
         invalidateConnection(sock);

         return;
      }

      // receive the rest of the message
      if(msgLength > numReceived)
         sock->recvExactT(&bufIn[numReceived], msgLength-numReceived, 0, recvTimeoutMS);

      // we got the complete message buffer => create msg object

      AbstractApp* app = PThread::getCurrentThreadApp();
      auto cfg = app->getCommonConfig();
      auto netMessageFactory = app->getNetMessageFactory();

      auto msg = netMessageFactory->createFromRaw(bufIn, msgLength);

      if(unlikely(msg->getMsgType() == NETMSGTYPE_Invalid) )
      { // message invalid
         LogContext(logContextStr).log(Log_NOTICE,
            std::string("Received an invalid message. Disconnecting: ") + sock->getPeername() );

         sock->unsetStats();
         invalidateConnection(sock);
         return;
      }

      // process the received msg

      bool processRes = false;

      if(likely(!cfg->getConnAuthHash() ||
         sock->getIsAuthenticated() ||
         (msg->getMsgType() == NETMSGTYPE_AuthenticateChannel) ) )
      { // auth disabled or channel is auth'ed or this is an auth msg => process
         NetMessage::ResponseContext rctx(NULL, sock, bufOut, bufOutLen, &stats);
         processRes = msg->processIncoming(rctx);
      }
      else
         LogContext(logContextStr).log(Log_NOTICE,
            std::string("Rejecting message from unauthenticated peer: ") + sock->getPeername() );

      // processing completed => cleanup

      sock->unsetStats();

      if(unlikely(!processRes) )
      {
         LogContext(logContextStr).log(Log_NOTICE,
            std::string("Problem encountered during processing of a message. Disconnecting: ") +
            sock->getPeername() );

         invalidateConnection(sock);
         return;
      }

      // processing completed successfully

      if(checkRDMASocketImmediateData(streamListener, sock) )
      { // immediate data available => do not return the socket to the streamListener
         // (new work has been added directly)
         return;
      }

      // stream socket => return the socket to the StreamListener
      streamListener->getSockReturnFD()->write(&sock, sizeof(Socket*) );

      return;

   }
   catch(SocketTimeoutException& e)
   {
      LogContext(logContextStr).log(Log_NOTICE,
         std::string("Connection timed out: ") + sock->getPeername() );
   }
   catch(SocketDisconnectException& e)
   {
      // (note: level Log_DEBUG to avoid spamming the log until we have log topics)
      LogContext(logContextStr).log(Log_DEBUG, std::string(e.what() ) );
   }
   catch(SocketException& e)
   {
      LogContext(logContextStr).log(Log_NOTICE,
         std::string("Connection error: ") + sock->getPeername() + std::string(": ") +
         std::string(e.what() ) );
   }

   sock->unsetStats();
   invalidateConnection(sock);
}

void IncomingDataWork::invalidateConnection(Socket* sock)
{
   const int recvTimeoutMS = 500;

   try
   {
      sock->shutdownAndRecvDisconnect(recvTimeoutMS);
   }
   catch(SocketException& e)
   {
      // don't care, because the conn is invalid anyway
   }

   delete(sock);
}

/**
 * Checks whether there is more immediate data available on an RDMASocket.
 *
 * If immediate data is available, a new Work package will be added to the Queue.
 * The reason for this is the fact that the filedescriptor will only send a notification
 * in case of new incoming data, so we need to check for old incoming data (that has already
 * been buffered internally by our impl) here.
 *
 * @return true if immediate data is available or if a conn error occurred (meaning the socket
 *    should not be returned to the streamListener)
 */
bool IncomingDataWork::checkRDMASocketImmediateData(StreamListener* streamListener, Socket* sock)
{
   const char* logContextStr = "Work (incoming data => check RDMA immediate data)";

   if(sock->getSockType() != NICADDRTYPE_RDMA)
      return false;

   // we have an RDMASocket

   try
   {
      RDMASocket* rdmaSock = (RDMASocket*)sock;

      if(!rdmaSock->nonblockingRecvCheck() )
      { // no more data available at the moment
         LOG_DEBUG(logContextStr, Log_SPAM,
            std::string("No immediate data: ") + sock->getPeername() );
         return false;
      }

      // we have immediate data available => add to queue

      LOG_DEBUG(logContextStr, Log_SPAM,
         std::string("Incoming RDMA data from: ") + sock->getPeername() );

      IncomingDataWork* work = new IncomingDataWork(streamListener, sock);

      if(sock->getIsDirect() )
         streamListener->getWorkQueue()->addDirectWork(work);
      else
         streamListener->getWorkQueue()->addIndirectWork(work);

      return true;
   }
   catch(SocketDisconnectException& e)
   {
      LogContext(logContextStr).log(Log_DEBUG, std::string(e.what() ) );
   }
   catch(SocketException& e)
   {
      LogContext(logContextStr).log(Log_NOTICE,
         std::string("Connection error: ") + sock->getPeername() + std::string(": ") +
         std::string(e.what() ) );
   }

   // conn error occurred

   delete(sock);

   return true;
}




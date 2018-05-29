#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/Worker.h>
#include <common/threading/PThread.h>
#include <common/net/message/NetMessage.h>
#include <common/net/sock/NamedSocket.h>
#include "CacheConnWorker.h"



CacheConnWorker::CacheConnWorker(const std::string& workerID, const std::string& namedSocketPath)
    : UnixConnWorker(workerID, "CacheConnWorker"),
      bufIn(NULL),
      bufOut(NULL)
{
   try
   {
      this->socket = new NamedSocket(namedSocketPath);

      applySocketOptions(socket);
   }
   catch(SocketException& e)
   {
      throw ComponentInitException(e.what() );
   }
}


CacheConnWorker::~CacheConnWorker()
{
   SAFE_DELETE(socket);

   SAFE_FREE(bufIn);
   SAFE_FREE(bufOut);
}

void CacheConnWorker::run()
{
   try
   {
      registerSignalHandler();

      initBuffers();

      workLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

}

void CacheConnWorker::workLoop()
{
   log.log(Log_DEBUG, std::string("Ready (TID: ") + StringTk::uint64ToStr(System::getTID() ) + ")");

   try
   {
      while(!getSelfTerminate() )
      {
         //log.log(4, "Waiting for work...");
         try
         {

            if(socket->waitForIncomingData(5000) )
            {
               //log.log(4, "Got work");

               bool processRes = processIncomingData(
                  bufIn, WORKER_BUFIN_SIZE, bufOut, WORKER_BUFOUT_SIZE);
               if(!processRes)
               {
                  break;
               }

               LOG_DEBUG("CacheConnWorker::workLoop", 4, "Work processed");

            }
         }
         catch(SocketInterruptedPollException& e)
         {
            // ignore interruption, because the debugger causes this
         }
      } // end of while loop
   }
   catch(SocketException& e)
   {
      log.log(2, "Error occurred on the workerEndpoint socket: " + std::string(e.what() ) );
   }
}

bool CacheConnWorker::processIncomingData(
   char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   const int recvTimeoutMS = 5000;

   unsigned numReceived = 0;
   NetMessage* msg = NULL;
   HighResolutionStats stats; // ignored currently

   try
   {
      // receive at least the message header

      numReceived += socket->recvExactT(bufIn, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);

      unsigned msgLength = NetMessageHeader::extractMsgLengthFromBuf(bufIn, numReceived);

      if(msgLength > bufInLen)
      { // message too big
         LogContext("CacheConnWorker::processIncomingData").log(Log_NOTICE,
            std::string("Received a message that is too large. Disconnecting:  ") +
            socket->getPeername() );

         invalidateConnection();

         return false;
      }

      // receive the rest of the message

      if(msgLength > numReceived)
         socket->recvExactT(&bufIn[numReceived], msgLength-numReceived, 0, recvTimeoutMS);

      // we got the complete message => deserialize and process it

      msg = netMessageFactory->createFromBuf(bufIn, msgLength);

      if(msg->getMsgType() == NETMSGTYPE_Invalid)
      { // message invalid
         LogContext("CacheConnWorker::processIncomingData").log(Log_NOTICE,
            std::string("Received an invalid message. Disconnecting: ") +
            socket->getPeername() );

         invalidateConnection();
         delete(msg);

         return false;
      }

      NetMessage::ResponseContext rctx(NULL, socket, bufOut, bufOutLen, &stats);
      bool processRes = msg->processIncoming(rctx);
      if(!processRes)
      {
         LogContext("CacheConnWorker::processIncomingData").log(Log_NOTICE,
            std::string("Problem encountered during processing of a message. Disconnecting: ") +
            socket->getPeername() );

         invalidateConnection();
         delete(msg);

         return false;
      }

      // completed successfully.
      delete(msg);

      return true;

   }
   catch(SocketTimeoutException& e)
   {
      LogContext("CacheConnWorker::processIncomingData").log(Log_NOTICE,
         std::string("Connection timed out: ") + socket->getPeername() );

   }
   catch(SocketDisconnectException& e)
   {
      LogContext("CacheConnWorker::processIncomingData").log(Log_NOTICE, std::string(e.what() ) );
   }
   catch(SocketException& e)
   {
      LogContext("CacheConnWorker::processIncomingData").log(Log_NOTICE,
         std::string("Connection error: ") + socket->getPeername() + std::string(": ") +
         std::string(e.what() ) );
   }

   invalidateConnection();
   SAFE_DELETE(msg);

   return false;
}

void CacheConnWorker::invalidateConnection()
{
   const int recvTimeoutMS = 2500;

   try
   {
      socket->shutdownAndRecvDisconnect(recvTimeoutMS);
   }
   catch(SocketException& e)
   {
      // don't care, because the conn is invalid anyway
   }
}

void CacheConnWorker::applySocketOptions(StandardSocket* sock)
{
   LogContext log("CacheConnWorker::applySocketOptions");

   try
   {
      sock->setSoKeepAlive(true);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to enable SoKeepAlive");
   }
}

/**
 * Note: For delayed buffer allocation during run(), because of NUMA.
 */
void CacheConnWorker::initBuffers()
{
   void* bufInVoid = NULL;
   void* bufOutVoid = NULL;

   int inAllocRes = posix_memalign(&bufInVoid, sysconf(_SC_PAGESIZE), WORKER_BUFIN_SIZE);
   int outAllocRes = posix_memalign(&bufOutVoid, sysconf(_SC_PAGESIZE), WORKER_BUFOUT_SIZE);

   IGNORE_UNUSED_VARIABLE(inAllocRes);
   IGNORE_UNUSED_VARIABLE(outAllocRes);

   this->bufIn = (char*)bufInVoid;
   this->bufOut = (char*)bufOutVoid;
}


#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/NetMessage.h>
#include "LocalConnWorker.h"

LocalConnWorker::LocalConnWorker(std::string workerID)
   throw(ComponentInitException) :
   PThread(std::string("LocalConnWorker") + workerID)
{
   log.setContext(std::string("LocalConnWorker") + workerID);
   
   this->netMessageFactory = PThread::getCurrentThreadApp()->getNetMessageFactory();

   this->bufIn = NULL;
   this->bufOut = NULL;
   
   this->available = false;
   
   try
   {
      StandardSocket::createSocketPair(PF_UNIX, SOCK_STREAM, 0, &workerEndpoint, &clientEndpoint);
      
      applySocketOptions(workerEndpoint);
      applySocketOptions(clientEndpoint);
   }
   catch(SocketException& e)
   {
      throw ComponentInitException(e.what() );
   }
}

LocalConnWorker::~LocalConnWorker()
{
   SAFE_DELETE(workerEndpoint);
   SAFE_DELETE(clientEndpoint);
   
   SAFE_FREE(bufIn);
   SAFE_FREE(bufOut);
}

void LocalConnWorker::run()
{
   try
   {
      registerSignalHandler();

      initBuffers();

      workLoop();
      
      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
   
}

void LocalConnWorker::workLoop()
{
   log.log(4, std::string("Ready (TID: ") + StringTk::uint64ToStr(System::getTID() ) + ")");

   try
   {
      while(!getSelfTerminate() )
      {
         //log.log(4, "Waiting for work...");
         try
         {
   
            if(workerEndpoint->waitForIncomingData(5000) )
            {
               //log.log(4, "Got work");
      
               bool processRes = processIncomingData(
                  bufIn, WORKER_BUFIN_SIZE, bufOut, WORKER_BUFOUT_SIZE);
               if(!processRes)
               {
                  break;
               }
               
               LOG_DEBUG("LocalConnWorker::workLoop", 4, "Work processed");
               
            }
         }
         catch(SocketInterruptedPollException& e)
         {
            // ignore iterruption, because the debugger causes this
         }
      } // end of while loop
   }
   catch(SocketException& e)
   {
      log.log(2, "Error occurred on the workerEndpoint socket: " + std::string(e.what() ) );
   }
}

bool LocalConnWorker::processIncomingData(
   char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   const int recvTimeoutMS = 5000;
   
   unsigned numReceived = 0;
   NetMessage* msg = NULL;
   HighResolutionStats stats; // ignored currently
   
   try
   {
      // receive at least the message header
      
      numReceived += workerEndpoint->recvExactT(bufIn, NETMSG_MIN_LENGTH, 0, recvTimeoutMS);
      
      unsigned msgLength = NetMessage::extractMsgLengthFromBuf(bufIn);
      
      if(msgLength > bufInLen)
      { // message too big
         LogContext("LocalConnWorker::processIncomingData").log(3,
            std::string("Received a message that is too large. Disconnecting:  ") +
            workerEndpoint->getPeername() );

         invalidateConnection();
         
         return false;
      }
      
      // receive the rest of the message

      if(msgLength > numReceived)
         workerEndpoint->recvExactT(&bufIn[numReceived], msgLength-numReceived, 0, recvTimeoutMS);
         
      // we got the complete message => deserialize and process it
      
      msg = netMessageFactory->createFromBuf(bufIn, msgLength);
      
      if(msg->getMsgType() == NETMSGTYPE_Invalid)
      { // message invalid
         LogContext("LocalConnWorker::processIncomingData").log(3,
            std::string("Received an invalid message. Disconnecting: ") +
            workerEndpoint->getPeername() );

         invalidateConnection();
         delete(msg);

         return false;
      }
      
      bool processRes = msg->processIncoming(NULL, workerEndpoint, bufOut, bufOutLen, &stats);
      if(!processRes)
      {
         LogContext("LocalConnWorker::processIncomingData").log(3,
            std::string("Problem encountered during processing of a message. Disconnecting: ") +
            workerEndpoint->getPeername() );

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
      LogContext("LocalConnWorker::processIncomingData").log(3,
         std::string("Connection timed out: ") + workerEndpoint->getPeername() );
         
   }
   catch(SocketDisconnectException& e)
   {
      LogContext("LocalConnWorker::processIncomingData").log(3, std::string(e.what() ) );
   }
   catch(SocketException& e)
   {
      LogContext("LocalConnWorker::processIncomingData").log(3,
         std::string("Connection error: ") + workerEndpoint->getPeername() + std::string(": ") +
         std::string(e.what() ) );
   }
   
   invalidateConnection();
   SAFE_DELETE(msg);
   
   return false;   
}

void LocalConnWorker::invalidateConnection()
{
   const int recvTimeoutMS = 2500;

   try
   {
      workerEndpoint->shutdownAndRecvDisconnect(recvTimeoutMS);
   }
   catch(SocketException& e)
   {
      // don't care, because the conn is invalid anyway
   }
}

void LocalConnWorker::applySocketOptions(StandardSocket* sock)
{
   LogContext log("LocalConnWorker::applySocketOptions");
   
   try
   {
      sock->setSoKeepAlive(true);
   }
   catch(SocketException& e)
   {
      log.log(3, "Failed to enable SoKeepAlive");
   }
}

/**
 * Note: For delayed buffer allocation during run(), because of NUMA.
 */
void LocalConnWorker::initBuffers()
{
   void* bufInVoid;
   void* bufOutVoid;

   int inAllocRes = posix_memalign(&bufInVoid, sysconf(_SC_PAGESIZE), WORKER_BUFIN_SIZE);
   int outAllocRes = posix_memalign(&bufOutVoid, sysconf(_SC_PAGESIZE), WORKER_BUFOUT_SIZE);

   IGNORE_UNUSED_VARIABLE(inAllocRes);
   IGNORE_UNUSED_VARIABLE(outAllocRes);

   this->bufIn = (char*)bufInVoid;
   this->bufOut = (char*)bufOutVoid;
}


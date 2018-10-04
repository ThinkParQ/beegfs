#include <common/app/AbstractApp.h>
#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/toolkit/StringTk.h>
#include "StreamListenerV2.h"

#include <sys/epoll.h>


#define EPOLL_EVENTS_NUM             (512) /* make it big to avoid starvation of higher FDs */
#define RDMA_CHECK_FORCE_POLLLOOPS   (7200) /* to avoid calling the check routine in each loop */
#define RDMA_CHECK_INTERVAL_MS       (150*60*1000) /* 150mins (must be more than double of the
                                     client-side idle disconnect interval to avoid cases where
                                     server disconnects first) */
#define SOCKRETURN_SOCKS_NUM         (32)


StreamListenerV2::StreamListenerV2(const std::string& listenerID, AbstractApp* app,
   StreamListenerWorkQueue* workQueue)
    : PThread(listenerID),
      app(app),
      log("StreamLisV2"),
      workQueue(workQueue),
      rdmaCheckForceCounter(0),
      useAggressivePoll(false)
{
   int epollCreateSize = 10; // size "10" is just a hint (and is actually ignored since Linux 2.6.8)
   this->epollFD = epoll_create(epollCreateSize);
   if(epollFD == -1)
   {
      throw ComponentInitException(std::string("Error during epoll_create(): ") +
         System::getErrString() );
   }

   if(!initSockReturnPipe() )
      throw ComponentInitException("Unable to initialize sock return pipe");
}

StreamListenerV2::~StreamListenerV2()
{
   delete(sockReturnPipe);

   deleteAllConns();

   if(epollFD != -1)
      close(epollFD);
}

bool StreamListenerV2::initSockReturnPipe()
{
   this->sockReturnPipe = new Pipe(false, true);

   struct epoll_event epollEvent;
   epollEvent.events = EPOLLIN;
   epollEvent.data.ptr = sockReturnPipe->getReadFD();
   if(epoll_ctl(epollFD, EPOLL_CTL_ADD, sockReturnPipe->getReadFD()->getFD(), &epollEvent) == -1)
   {
      log.logErr(std::string("Unable to add sock return pipe (read side) to epoll set: ") +
         System::getErrString() );
      return false;
   }

   return true;
}

void StreamListenerV2::run()
{
   try
   {
      registerSignalHandler();

      listenLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void StreamListenerV2::listenLoop()
{
   const int epollTimeoutMS = useAggressivePoll ? 0 : 3000;

   struct epoll_event epollEvents[EPOLL_EVENTS_NUM];

   // (just to have these values on the stack...)
   const int epollFD = this->epollFD;
   FileDescriptor* sockReturnPipeReadEnd = this->sockReturnPipe->getReadFD();

   bool runRDMAConnIdleCheck = false; // true just means we call the method (not enforce the check)

   // wait for incoming events and handle them...

   while(!getSelfTerminate() )
   {
      //log.log(Log_DEBUG, std::string("Before poll(). pollArrayLen: ") +
      //   StringTk::uintToStr(pollArrayLen) );

      int epollRes = epoll_wait(epollFD, epollEvents, EPOLL_EVENTS_NUM, epollTimeoutMS);

      if(unlikely(epollRes < 0) )
      { // error occurred
         if(errno == EINTR) // ignore interruption, because the debugger causes this
            continue;

         log.logErr(std::string("Unrecoverable epoll_wait error: ") + System::getErrString() );
         break;
      }
      else
      if(unlikely(!epollRes || (rdmaCheckForceCounter++ > RDMA_CHECK_FORCE_POLLLOOPS) ) )
      { // epollRes==0 is nothing to worry about, just idle

         // note: we can't run idle check here directly because the check might modify the
         //    poll set, which will be accessed in the loop below
         runRDMAConnIdleCheck = true;
      }

      // handle incoming data & connection attempts
      for(size_t i=0; i < (size_t)epollRes; i++)
      {
         struct epoll_event* currentEvent = &epollEvents[i];
         Pollable* currentPollable = (Pollable*)currentEvent->data.ptr;

         if(currentPollable == sockReturnPipeReadEnd)
            onSockReturn();
         else
            onIncomingData( (Socket*)currentPollable);
      }

      if(unlikely(runRDMAConnIdleCheck) )
      { // note: whether check actually happens depends on elapsed time since last check
         runRDMAConnIdleCheck = false;
         rdmaConnIdleCheck();
      }

   }
}


/**
 * Receive msg header and add the socket to the work queue.
 */
void StreamListenerV2::onIncomingData(Socket* sock)
{
   // check whether this is just a false alarm from a RDMASocket
   if( (sock->getSockType() == NICADDRTYPE_RDMA) &&
      isFalseAlarm( (RDMASocket*)sock) )
   {
      return;
   }

   try
   {
      const int recvTimeoutMS = 5000;

      char msgHeaderBuf[NETMSG_HEADER_LENGTH];
      NetMessageHeader msgHeader;

      // receive & deserialize message header

      sock->recvExactT(msgHeaderBuf, NETMSG_HEADER_LENGTH, 0, recvTimeoutMS);

      NetMessage::deserializeHeader(msgHeaderBuf, NETMSG_HEADER_LENGTH, &msgHeader);

      /* (note on header verification: we leave header verification work to the worker threads to
         save CPU cycles in the stream listener and instead just take what we need to know here, no
         matter whether the header is valid or not.) */

      // create work and add it to queue

      //log.log(Log_DEBUG, "Creating new work for to the queue");

      IncomingPreprocessedMsgWork* work = new IncomingPreprocessedMsgWork(app, sock, &msgHeader);

      int sockFD = sock->getFD(); /* note: we store this here for delayed pollList removal, because
            worker thread might disconnect, so the sock gets deleted by the worker and thus "sock->"
            pointer becomes invalid */

      sock->setHasActivity(); // mark sock as active (for idle disconnect check)

      // (note: userID intToStr (not uint) because default userID (~0) looks better this way)
      LOG_DEBUG("StreamListenerV2::onIncomingData", Log_DEBUG,
         "Incoming message: " + netMessageTypeToStr(msgHeader.msgType) + "; "
         "from: " + sock->getPeername() + "; "
         "userID: " + StringTk::intToStr(msgHeader.msgUserID) +
         (msgHeader.msgTargetID
            ? "; targetID: " + StringTk::uintToStr(msgHeader.msgTargetID)
            : "") );

      if (sock->getIsDirect())
         getWorkQueue(msgHeader.msgTargetID)->addDirectWork(work, msgHeader.msgUserID);
      else
         getWorkQueue(msgHeader.msgTargetID)->addIndirectWork(work, msgHeader.msgUserID);

      /* notes on sock handling:
         *) no need to remove sock from epoll set, because we use edge-triggered mode with
            oneshot flag (which disables further events after first one has been reported).
         *) a sock that is closed by a worker is not a problem, because it will automatically be
            removed from the epoll set by the kernel.
         *) we just need to re-arm the epoll entry upon sock return. */

      pollList.removeByFD(sockFD);

      return;

   }
   catch(SocketTimeoutException& e)
   {
      log.log(Log_NOTICE, "Connection timed out: " + sock->getPeername() );
   }
   catch(SocketDisconnectException& e)
   {
      // (note: level Log_DEBUG here to avoid spamming the log until we have log topics)
      log.log(Log_DEBUG, std::string(e.what() ) );
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE,
         "Connection error: " + sock->getPeername() + ": " + std::string(e.what() ) );
   }

   // socket exception occurred => cleanup

   pollList.removeByFD(sock->getFD() );

   IncomingPreprocessedMsgWork::invalidateConnection(sock); // also includes delete(sock)
}

/**
 * Receive pointer to returned socket through the sockReturnPipe and re-add it to the pollList.
 */
void StreamListenerV2::onSockReturn()
{
   SockReturnPipeInfo returnInfos[SOCKRETURN_SOCKS_NUM];

   // try to get multiple returnInfos at once (if available)

   ssize_t readRes = sockReturnPipe->getReadFD()->read(&returnInfos, sizeof(SockReturnPipeInfo) );

   // loop: walk over each info and handle the contained socket

   for(size_t i=0; ; i++)
   {
      SockReturnPipeInfo& currentReturnInfo = returnInfos[i];

      // make sure we have a complete SockReturnPipeInfo

      if(unlikely(readRes < (ssize_t)sizeof(SockReturnPipeInfo) ) )
      { // only got a partial SockReturnPipeInfo => recv the rest
         char* currentReturnInfoChar = (char*)&currentReturnInfo;

         sockReturnPipe->getReadFD()->readExact(
            &currentReturnInfoChar[readRes], sizeof(SockReturnPipeInfo)-readRes);

         readRes = sizeof(SockReturnPipeInfo);
      }

      // handle returnInfo depending on contained returnType...

      Socket* currentSock = currentReturnInfo.sock;
      SockPipeReturnType returnType = currentReturnInfo.returnType;

      switch(returnType)
      {
         case SockPipeReturn_MSGDONE_NOIMMEDIATE:
         { // most likely case: worker is done with a msg and now returns the sock to the epoll set

            struct epoll_event epollEvent;
            epollEvent.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
            epollEvent.data.ptr = currentSock;

            int epollRes = epoll_ctl(epollFD, EPOLL_CTL_MOD, currentSock->getFD(), &epollEvent);

            if(likely(!epollRes) )
            { // sock was successfully re-armed in epoll set
               pollList.add(currentSock);

               break; // break out of switch
            }
            else
            if(errno != ENOENT)
            { // error
               log.logErr("Unable to re-arm sock in epoll set. "
                  "FD: " + StringTk::uintToStr(currentSock->getFD() ) + "; "
                  "SockTypeNum: " + StringTk::uintToStr(currentSock->getSockType() ) + "; "
                  "SysErr: " + System::getErrString() );
               log.log(Log_NOTICE, "Disconnecting: " + currentSock->getPeername() );

               delete(currentSock);

               break; // break out of switch
            }

            /* for ENOENT, we fall through to NEWCONN, because this socket appearently wasn't
               used with this stream listener yet, so we need to add it (instead of modify it) */

         } // might fall through here on ENOENT
         BEEGFS_FALLTHROUGH;

         case SockPipeReturn_NEWCONN:
         { // new conn from ConnAcceptor (or wasn't used with this stream listener yet)

            // add new socket file descriptor to epoll set

            struct epoll_event epollEvent;
            epollEvent.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
            epollEvent.data.ptr = currentSock;

            int epollRes = epoll_ctl(epollFD, EPOLL_CTL_ADD, currentSock->getFD(), &epollEvent);
            if(likely(!epollRes) )
            { // socket was successfully added to epoll set
               pollList.add(currentSock);
            }
            else
            { // adding to epoll set failed => unrecoverable error
               log.logErr("Unable to add sock to epoll set. "
                  "FD: " + StringTk::uintToStr(currentSock->getFD() ) + " "
                  "SockTypeNum: " + StringTk::uintToStr(currentSock->getSockType() ) + " "
                  "SysErr: " + System::getErrString() );
               log.log(Log_NOTICE, "Disconnecting: " + currentSock->getPeername() );

               delete(currentSock);
            }

         } break;

         case SockPipeReturn_MSGDONE_WITHIMMEDIATE:
         { // special case: worker detected that immediate data is available after msg processing
            // data immediately available => recv header and so on
            onIncomingData(currentSock);
         } break;

         default:
         { // should never happen: unknown/unhandled returnType
            log.logErr("Should never happen: "
               "Unknown socket return type: " + StringTk::uintToStr(returnType) );
         } break;

      } // end of switch(returnType)



      readRes -= sizeof(SockReturnPipeInfo);
      if(!readRes)
         break; // all received returnInfos have been processed

   } // end of "for each received SockReturnPipeInfo" loop

}

/**
 * Does not really check but instead just drops connections that have been idle for some time.
 * Note: Actual checking is not performed to avoid timeout delays in the StreamListenerV2.
 */
void StreamListenerV2::rdmaConnIdleCheck()
{
   const unsigned checkIntervalMS = RDMA_CHECK_INTERVAL_MS;

   if(rdmaCheckT.elapsedMS() < checkIntervalMS)
   {
      this->rdmaCheckForceCounter = 0;
      return;
   }

   int numCheckedConns = 0;
   IntList disposalFDs;
   PollMap* pollMap = pollList.getPollMap();


   // walk over all rdma socks, check conn status, delete idle socks and add them to
   //    the disposal list (note: we do not modify the pollList/pollMap yet)

   for(PollMapIter iter = pollMap->begin(); iter != pollMap->end(); iter++)
   {
      Socket* currentSock = (Socket*)iter->second;

      if(currentSock->getSockType() != NICADDRTYPE_RDMA)
         continue;

      numCheckedConns++;

      // rdma socket => check activity
      if(currentSock->getHasActivity() )
      { // conn had activity since last check => reset activity flag
         currentSock->resetHasActivity();
      }
      else
      { // conn was inactive the whole time => disconnect and add to disposal list
         log.log(Log_DEBUG,
            "Disconnecting idle RDMA connection: " + currentSock->getPeername() );

         delete(currentSock);
         disposalFDs.push_back(iter->first);
      }

   }


   // remove idle socks from pollMap

   for(IntListIter iter = disposalFDs.begin(); iter != disposalFDs.end(); iter++)
   {
      pollList.removeByFD(*iter);
   }

   if (!disposalFDs.empty())
   { // logging
      LOG_DEBUG("StreamListenerV2::rdmaConnIdleCheck", Log_SPAM, std::string("Check results: ") +
         std::string("disposed ") + StringTk::int64ToStr(disposalFDs.size() ) + "/" +
         StringTk::int64ToStr(numCheckedConns) );
      IGNORE_UNUSED_VARIABLE(numCheckedConns);
   }


   // reset time counter
   rdmaCheckT.setToNow();
}

/**
 * RDMA sockets might signal incoming events that are not actually incoming data, but instead
 * handled internally in the IBVSocket class.
 * In the latter case, we should not call recv() on the socket, because that would hang. So we need
 * this method to decide wheter we put this socket into the incoming data queue or not.
 *
 * Note: A special case is a connection error that is detected during the false alarm
 * check. We return true here, because the disconnect is handled internally so that
 * no further steps are required by the caller.
 *
 * @return true in case of a false alarm (which means that no incoming data for recv is
 * immediately available) or in case of an error; false otherwise (i.e. the caller can call recv()
 * on this socket without blocking)
 */
bool StreamListenerV2::isFalseAlarm(RDMASocket* sock)
{
   const char* logContext = "StreamListenerV2 (incoming RDMA check)";

   try
   {
      if(!sock->nonblockingRecvCheck() )
      { // false alarm
         LOG_DEBUG(logContext, Log_SPAM,
            std::string("Ignoring false alarm from: ") + sock->getPeername() );

         // we use one-shot mechanism, so we need to re-arm the socket after an event notification

         struct epoll_event epollEvent;
         epollEvent.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
         epollEvent.data.ptr = sock;

         int epollRes = epoll_ctl(this->epollFD, EPOLL_CTL_MOD, sock->getFD(), &epollEvent);
         if(unlikely(epollRes == -1) )
         { // error
            log.logErr("Unable to re-arm socket in epoll set: " + System::getErrString() );
            log.log(Log_NOTICE, "Disconnecting: " + sock->getPeername() );

            delete(sock);
         }

         return true;
      }

      // we really have incoming data

      return false;
   }
   catch(SocketDisconnectException& e)
   { // a disconnect here is nothing to worry about
      LogContext(logContext).log(Log_DEBUG,
         std::string(e.what() ) );
   }
   catch(SocketException& e)
   {
      LogContext(logContext).log(Log_NOTICE,
         std::string("Connection error: ") + sock->getPeername() + std::string(": ") +
         std::string(e.what() ) );
   }

   // conn error occurred

   pollList.remove(sock);

   delete(sock);

   return true;
}

void StreamListenerV2::deleteAllConns()
{
   PollMap* pollMap = pollList.getPollMap();

   for(PollMapIter iter = pollMap->begin(); iter != pollMap->end(); iter++)
   {
      delete(iter->second);
   }
}

#include <common/app/AbstractApp.h>
#include <common/toolkit/StringTk.h>
#include "worker/IncomingDataWork.h"
#include "StreamListener.h"

#include <sys/epoll.h>


#define EPOLL_EVENTS_NUM             (512) /* make it big to avoid starvation of higher FDs */
#define RDMA_CHECK_FORCE_POLLLOOPS   (7200) /* to avoid calling the check routine in each loop */
#define RDMA_CHECK_INTERVAL_MS       (150*60*1000) /* 150mins (must be more than double of the
                                     client-side idle disconnect interval to avoid cases where
                                     server disconnects first) */
#define SOCKRETURN_SOCKS_NUM         (32)


StreamListener::StreamListener(NicAddressList& localNicList, MultiWorkQueue* workQueue,
   unsigned short listenPort)
    : PThread("StreamLis"),
      log("StreamLis"),
      workQueue(workQueue),
      rdmaCheckForceCounter(0)
{
   NicListCapabilities localNicCaps;
   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);

   this->epollFD = epoll_create(10); // "10" is just a hint (and is actually ignored)
   if(epollFD == -1)
   {
      throw ComponentInitException(std::string("Error during epoll_create(): ") +
         System::getErrString() );
   }

   if(!initSockReturnPipe() )
      throw ComponentInitException("Unable to initialize sock return pipe");

   if(!initSocks(listenPort, &localNicCaps) )
      throw ComponentInitException("Unable to initialize socket");
}

StreamListener::~StreamListener()
{
   pollList.remove(sockReturnPipe->getReadFD() ); // may not be deleted by deleteAllConns()
   delete(sockReturnPipe);

   deleteAllConns();

   if(epollFD != -1)
      close(epollFD);


   //SAFE_DELETE(tcpListenSock); // deleted through pollList now
   //SAFE_DELETE(sdpListenSock); // deleted through pollList now
   //SAFE_DELETE(rdmaListenSock); // deleted through pollList now
}

bool StreamListener::initSockReturnPipe()
{
   this->sockReturnPipe = new Pipe(false, true);

   pollList.add(sockReturnPipe->getReadFD() );

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

bool StreamListener::initSocks(unsigned short listenPort, NicListCapabilities* localNicCaps)
{
   auto cfg = PThread::getCurrentThreadApp()->getCommonConfig();

   rdmaListenSock = NULL;
   sdpListenSock = NULL;
   tcpListenSock = NULL;


   // RDMA

   if(localNicCaps->supportsRDMA)
   { // RDMA usage is enabled
      try
      {
         rdmaListenSock = RDMASocket::create().release();
         rdmaListenSock->bind(listenPort);
         rdmaListenSock->listen();

         pollList.add(rdmaListenSock);

         struct epoll_event epollEvent;
         epollEvent.events = EPOLLIN;
         epollEvent.data.ptr = rdmaListenSock;
         if(epoll_ctl(epollFD, EPOLL_CTL_ADD, rdmaListenSock->getFD(), &epollEvent) == -1)
         {
            log.logErr(std::string("Unable to add RDMA listen sock to epoll set: ") +
               System::getErrString() );
            return false;
         }

         log.log(3, std::string("Listening for RDMA connections: Port ") +
            StringTk::intToStr(listenPort) );
      }
      catch(SocketException& e)
      {
         log.logErr(std::string("RDMA socket: ") + e.what() );
         return false;
      }
   }

   // SDP

   if(localNicCaps->supportsSDP)
   { // SDP usage is enabled
      try
      {
         sdpListenSock = new StandardSocket(PF_SDP, SOCK_STREAM);
         sdpListenSock->setSoReuseAddr(true);
         sdpListenSock->bind(listenPort);
         sdpListenSock->listen();

         pollList.add(sdpListenSock);

         struct epoll_event epollEvent;
         epollEvent.events = EPOLLIN;
         epollEvent.data.ptr = sdpListenSock;
         if(epoll_ctl(epollFD, EPOLL_CTL_ADD, sdpListenSock->getFD(), &epollEvent) == -1)
         {
            log.logErr(std::string("Unable to add SDP listen sock to epoll set: ") +
               System::getErrString() );
            return false;
         }

         log.log(Log_NOTICE, std::string("Listening for SDP connections: Port ") +
            StringTk::intToStr(listenPort) );
      }
      catch(SocketException& e)
      {
         log.logErr(std::string("SDP socket: ") + e.what() );
         return false;
      }
   }

   // TCP

   try
   {
      tcpListenSock = new StandardSocket(PF_INET, SOCK_STREAM);
      tcpListenSock->setSoReuseAddr(true);
      int bufsize = cfg->getConnTCPRcvBufSize();
      if (bufsize > 0)
         tcpListenSock->setSoRcvBuf(bufsize);
      tcpListenSock->bind(listenPort);
      tcpListenSock->listen();

      pollList.add(tcpListenSock);
      struct epoll_event epollEvent;
      epollEvent.events = EPOLLIN;
      epollEvent.data.ptr = tcpListenSock;
      if(epoll_ctl(epollFD, EPOLL_CTL_ADD, tcpListenSock->getFD(), &epollEvent) == -1)
      {
         log.logErr(std::string("Unable to add TCP listen sock to epoll set: ") +
            System::getErrString() );
         return false;
      }

      log.log(Log_NOTICE, std::string("Listening for TCP connections: Port ") +
         StringTk::intToStr(listenPort) );
   }
   catch(SocketException& e)
   {
      log.logErr(std::string("TCP socket: ") + e.what() );
      return false;
   }


   return true;
}


void StreamListener::run()
{
   try
   {
      registerSignalHandler();

      listenLoop();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void StreamListener::listenLoop()
{
   const int epollTimeoutMS = 3000;

   struct epoll_event epollEvents[EPOLL_EVENTS_NUM];

   // (just to have 'em on the stack)
   const int epollFD = this->epollFD;
   RDMASocket* rdmaListenSock = this->rdmaListenSock;
   StandardSocket* sdpListenSock = this->sdpListenSock;
   StandardSocket* tcpListenSock = this->tcpListenSock;
   FileDescriptor* sockReturnPipeReadEnd = this->sockReturnPipe->getReadFD();

   bool runRDMAConnIdleCheck = false; // true just means we call the method (not enforce the check)

   // wait for incoming events and handle them...

   while(!getSelfTerminate() )
   {
      //log.log(4, std::string("Before poll(). pollArrayLen: ") +
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

         //log.log(4, std::string("Incoming data on FD: ") +
         //   StringTk::intToStr(pollArray[i].fd) ); // debug in

         if(unlikely(currentPollable == rdmaListenSock) )
            onIncomingRDMAConnection(rdmaListenSock);
         else
         if(unlikely(currentPollable == sdpListenSock) )
            onIncomingStandardConnection(sdpListenSock);
         else
         if(unlikely(currentPollable == tcpListenSock) )
            onIncomingStandardConnection(tcpListenSock);
         else
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
 * Accept the incoming connection and add the new socket to the pollList.
 */
void StreamListener::onIncomingStandardConnection(StandardSocket* sock)
{
   try
   {
      struct sockaddr_in peerAddr;
      socklen_t peerAddrLen = sizeof(peerAddr);

      std::unique_ptr<StandardSocket> acceptedSock(
         (StandardSocket*)sock->accept( (struct sockaddr*)&peerAddr, &peerAddrLen));

      // (note: level Log_DEBUG to avoid spamming the log until we have log topics)
      log.log(Log_DEBUG, std::string("Accepted new connection from " +
         Socket::endpointAddrToString(&peerAddr.sin_addr, ntohs(peerAddr.sin_port) ) ) +
         std::string(" [SockFD: ") + StringTk::intToStr(acceptedSock->getFD() ) +
         std::string("]") );

      applySocketOptions(acceptedSock.get());

      struct epoll_event epollEvent;
      epollEvent.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
      epollEvent.data.ptr = acceptedSock.get();

      if(epoll_ctl(this->epollFD, EPOLL_CTL_ADD, acceptedSock->getFD(), &epollEvent) == -1)
      {
         throw SocketException(std::string("Unable to add sock to epoll set: ") +
            System::getErrString() );
      }

      pollList.add(acceptedSock.release());
   }
   catch(SocketException& se)
   {
      log.logErr(std::string("Trying to continue after connection accept error: ") + se.what() );
   }
}

/**
 * Accept the incoming connection and add the new socket to the pollList.
 */
void StreamListener::onIncomingRDMAConnection(RDMASocket* sock)
{
   // accept the incoming connection (and loop until no more delayed events are waiting on
   //    this socket)
   // (Note: RDMASockets use this internally also to handle other kindes of events)

   // loop: check whether this is a false alarm, try to accept a new connection (if
   //    available) and also check for further events after accept

   while(sock->checkDelayedEvents() )
   {
      try
      {
         struct sockaddr_in peerAddr;
         socklen_t peerAddrLen = sizeof(peerAddr);

         RDMASocket* acceptedSock =
            (RDMASocket*)sock->accept( (struct sockaddr*)&peerAddr, &peerAddrLen);

         // note: RDMASocket::accept() might return NULL (which is not an error)
         if(!acceptedSock)
         {
            log.log(4, std::string("Ignoring an internal event on the listening RDMA socket") );
            continue;
         }

         // (note: level Log_DEBUG to avoid spamming the log until we have log topics)
         log.log(Log_DEBUG, std::string("Accepted new RDMA connection from " +
            Socket::endpointAddrToString(&peerAddr.sin_addr, ntohs(peerAddr.sin_port) ) ) +
            std::string(" [SockFD: ") + StringTk::intToStr(acceptedSock->getFD() ) +
            std::string("]") );

         struct epoll_event epollEvent;
         epollEvent.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
         epollEvent.data.ptr = acceptedSock;

         if(epoll_ctl(this->epollFD, EPOLL_CTL_ADD, acceptedSock->getFD(), &epollEvent) == -1)
         {
            throw SocketException(std::string("Unable to add RDMA sock to epoll set: ") +
               System::getErrString() );
         }

         pollList.add(acceptedSock);
      }
      catch(SocketException& se)
      {
         log.logErr(std::string("Trying to continue after RDMA connection accept error: ") +
            se.what() );
      }

   }
}

/**
 * Add the socket to the workQueue.
 *
 * Note: Data can also be added to the queue by the
 *    IncomingDataWork::checkRDMASocketImmediateData() method.
 */
void StreamListener::onIncomingData(Socket* sock)
{
   // check whether this is just a false alarm from a RDMASocket
   if( (sock->getSockType() == NICADDRTYPE_RDMA) &&
      isFalseAlarm( (RDMASocket*)sock) )
   {
      return;
   }

   // we really have incoming data => add work to queue

   LOG_DEBUG("StreamListener::onIncomingData", 4,
      std::string("Incoming stream data from: ") + sock->getPeername() );

   int sockFD = sock->getFD(); // note: we store this here for delayed pollList removal, because
      // this might be disconnect work, so the sock gets deleted by the worker and thus "sock->"
      // becomes invalid

   //log.log(4, "Creating new work for to the queue");

   IncomingDataWork* work = new IncomingDataWork(this, sock);

   //log.log(4, "Adding new work to the queue");

   sock->setHasActivity(); // mark as active (for idle disconnect check)

   if(sock->getIsDirect() )
      workQueue->addDirectWork(work);
   else
      workQueue->addIndirectWork(work);

   //log.log(4, "Added new work to the queue");

   // note: no need to remove sock from epoll set, because we use edge-triggered mode with oneshot
      // flag (which disables further events after first one has been reported). a sock that is
      // closed by a worker is not a problem, because it will automatically be removed from the
      // epoll set by the kernel.
      // we just need to re-arm the epoll entry upon sock return.

   pollList.removeByFD(sockFD);
}

/**
 * Receive pointer to returned socket through the sockReturnPipe and
 * re-add it to the pollList.
 */
void StreamListener::onSockReturn()
{
   Socket* socks[SOCKRETURN_SOCKS_NUM];

   ssize_t readRes = sockReturnPipe->getReadFD()->read(&socks, sizeof(socks) );

   for(size_t i=0; ; i++)
   {
      Socket* currentSock = socks[i];

      if(unlikely(readRes < (ssize_t)sizeof(Socket*) ) )
      { // recv the rest of the socket pointer
         char* currentSockChar = (char*)currentSock;

         sockReturnPipe->getReadFD()->readExact(
            &currentSockChar[readRes], sizeof(Socket*)-readRes);

         readRes = sizeof(Socket*);
      }

      //LOG_DEBUG("StreamListener::onSockReturn", 5,
      //   std::string("Socket returned (through pipe). SockFD: ") +
      //   StringTk::intToStr(currentSock->getFD() ) );

      struct epoll_event epollEvent;
      epollEvent.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
      epollEvent.data.ptr = currentSock;

      int epollRes = epoll_ctl(this->epollFD, EPOLL_CTL_MOD, currentSock->getFD(), &epollEvent);
      if(unlikely(epollRes == -1) )
      { // error
         log.logErr("Unable to re-arm sock in epoll set: " + System::getErrString() );
         log.log(3, "Disconnecting: " + currentSock->getPeername() );

         delete(currentSock);
      }
      else
      {
         pollList.add(currentSock);
      }

      readRes -= sizeof(Socket*);
      if(!readRes)
         break;
   }

}

/**
 * Does not really check but instead just drops connections that have been idle for some time.
 * Note: Actual checking is not performed to avoid timeout delays in the StreamListener.
 */
void StreamListener::rdmaConnIdleCheck()
{
   const unsigned checkIntervalMS = RDMA_CHECK_INTERVAL_MS;

   if(!this->rdmaListenSock)
      return;

   if(rdmaCheckT.elapsedMS() < checkIntervalMS)
   {
      this->rdmaCheckForceCounter = 0;
      return;
   }

   //LOG_DEBUG("StreamListener::rdmaConnIdleCheck", 5, std::string("Performing check...") );

   int rdmaListenFD = rdmaListenSock->getFD();
   int numCheckedConns = 0;
   IntList disposalFDs;
   PollMap* pollMap = pollList.getPollMap();


   // walk over all rdma socks, check conn status, delete idle socks and add them to
   //    the disposal list (note: we do not modify the pollList/pollMap yet)

   for(PollMapIter iter = pollMap->begin(); iter != pollMap->end(); iter++)
   {
      int currentFD = iter->first;

      if(currentFD == sockReturnPipe->getReadFD()->getFD() )
         continue;

      Socket* currentSock = (Socket*)iter->second;

      if(currentSock->getSockType() != NICADDRTYPE_RDMA)
         continue;

      if(iter->first == rdmaListenFD)
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
      LOG_DEBUG("StreamListener::rdmaConnIdleCheck", 5, std::string("Check results: ") +
         std::string("disposed ") + StringTk::int64ToStr(disposalFDs.size() ) + "/" +
         StringTk::int64ToStr(numCheckedConns) );
      IGNORE_UNUSED_VARIABLE(numCheckedConns);
   }


   // reset time counter
   rdmaCheckT.setToNow();
}

void StreamListener::applySocketOptions(StandardSocket* sock)
{
   LogContext log("StreamListener (apply socket options)");

   try
   {
      sock->setTcpCork(false);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to disable TcpCork");
   }

   try
   {
      sock->setTcpNoDelay(true);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to enable TcpNoDelay");
   }

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
bool StreamListener::isFalseAlarm(RDMASocket* sock)
{
   const char* logContext = "StreamListener (incoming RDMA check)";

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

void StreamListener::deleteAllConns()
{
   PollMap* pollMap = pollList.getPollMap();

   for(PollMapIter iter = pollMap->begin(); iter != pollMap->end(); iter++)
   {
      delete(iter->second);
   }
}

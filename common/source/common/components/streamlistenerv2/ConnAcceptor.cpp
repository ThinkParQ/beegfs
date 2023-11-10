#include <common/app/AbstractApp.h>
#include <common/components/worker/IncomingDataWork.h>
#include <common/components/streamlistenerv2/StreamListenerV2.h>
#include <common/toolkit/StringTk.h>
#include "ConnAcceptor.h"

#include <sys/epoll.h>


#define EPOLL_EVENTS_NUM             (8) /* how many events we can take at once from epoll_wait */


ConnAcceptor::ConnAcceptor(AbstractApp* app, NicAddressList& localNicList,
   unsigned short listenPort)
    : PThread("ConnAccept"),
      app(app),
      log("ConnAccept"),
      listenPort(listenPort),
      localNicCapsUpdated(false)
{

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);

   int epollCreateSize = 10; // size "10" is just a hint (and is actually ignored since Linux 2.6.8)

   this->epollFD = epoll_create(epollCreateSize);
   if(epollFD == -1)
   {
      throw ComponentInitException(std::string("Error during epoll_create(): ") +
         System::getErrString() );
   }

   if(!initSocks() )
      throw ComponentInitException("Unable to initialize socket");
}

ConnAcceptor::~ConnAcceptor()
{
   if(epollFD != -1)
      close(epollFD);

   SAFE_DELETE(tcpListenSock);
   SAFE_DELETE(sdpListenSock);
   SAFE_DELETE(rdmaListenSock);
}

bool ConnAcceptor::startRDMASocket(NicListCapabilities* localNicCaps)
{
   auto cfg = PThread::getCurrentThreadApp()->getCommonConfig();
   if(localNicCaps->supportsRDMA)
   { // RDMA usage is enabled
      try
      {
         rdmaListenSock = RDMASocket::create().release();
         rdmaListenSock->setTimeouts(cfg->getConnRDMATimeoutConnect(),
            cfg->getConnRDMATimeoutFlowSend(), cfg->getConnRDMATimeoutPoll());
         rdmaListenSock->bind(listenPort);
         rdmaListenSock->listen();

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
         SAFE_DELETE(rdmaListenSock);
         return false;
      }
   }
   return true;
}

bool ConnAcceptor::startSDPSocket(NicListCapabilities* localNicCaps)
{
   if(localNicCaps->supportsSDP)
   { // SDP usage is enabled
      try
      {
         sdpListenSock = new StandardSocket(PF_SDP, SOCK_STREAM);
         sdpListenSock->setSoReuseAddr(true);
         sdpListenSock->bind(listenPort);
         sdpListenSock->listen();

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
         SAFE_DELETE(sdpListenSock);
         return false;
      }
   }
   return true;
}

bool ConnAcceptor::initSocks()
{
   auto cfg = PThread::getCurrentThreadApp()->getCommonConfig();

   rdmaListenSock = NULL;
   sdpListenSock = NULL;
   tcpListenSock = NULL;

   // RDMA
   if (!startRDMASocket(&localNicCaps))
      return false;

   // SDP
   if (!startSDPSocket(&localNicCaps))
      return false;

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
      SAFE_DELETE(tcpListenSock);
      return false;
   }

   return true;
}

void ConnAcceptor::updateLocalNicList(NicAddressList& localNicList)
{
   // we don't store the localNicList, just use it to update localNicCaps
   NicListCapabilities localNicCaps;
   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   const std::lock_guard<Mutex> lock(localNicCapsMutex);
   this->localNicCaps = localNicCaps;
   localNicCapsUpdated = true;
}

void ConnAcceptor::run()
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

void ConnAcceptor::handleNewLocalNicCaps()
{
   NicListCapabilities localNicCaps;
   bool localNicCapsUpdated;
   {
      const std::lock_guard<Mutex> lock(localNicCapsMutex);
      localNicCapsUpdated = this->localNicCapsUpdated;
      if (localNicCapsUpdated)
      {
         localNicCaps = this->localNicCaps;
         this->localNicCapsUpdated = false;
      }
   }

   if (localNicCapsUpdated)
   {
      if (localNicCaps.supportsRDMA)
      {
         if (!rdmaListenSock)
            startRDMASocket(&localNicCaps);
      }
      else if (rdmaListenSock)
      {
         log.log(Log_NOTICE, std::string("Shutdown RDMA listen sock..."));

         if(epoll_ctl(epollFD, EPOLL_CTL_DEL, rdmaListenSock->getFD(), NULL) == -1)
            log.logErr(std::string("Unable to remove RDMA listen sock from epoll set: ") +
               System::getErrString() );
         SAFE_DELETE(rdmaListenSock);

         log.log(Log_NOTICE, std::string("Shutdown RDMA listen sock complete"));
      }

      if (localNicCaps.supportsSDP)
      {
         if (!sdpListenSock)
            startSDPSocket(&localNicCaps);
      }
      else if (sdpListenSock)
      {
         log.log(Log_NOTICE, std::string("Shutdown SDP listen sock..."));

         if(epoll_ctl(epollFD, EPOLL_CTL_DEL, sdpListenSock->getFD(), NULL) == -1)
            log.logErr(std::string("Unable to remove SDP listen sock from epoll set: ") +
               System::getErrString() );
         SAFE_DELETE(sdpListenSock);

         log.log(Log_NOTICE, std::string("Shutdown SDP listen sock complete"));
      }

   }
}

void ConnAcceptor::listenLoop()
{
   const int epollTimeoutMS = 3000;

   struct epoll_event epollEvents[EPOLL_EVENTS_NUM];

   // (just to have these values on the stack...)
   const int epollFD = this->epollFD;
   RDMASocket* rdmaListenSock;
   StandardSocket* sdpListenSock;
   StandardSocket* tcpListenSock;

   // wait for incoming events and handle them...

   while(!getSelfTerminate() )
   {

      handleNewLocalNicCaps();
      rdmaListenSock = this->rdmaListenSock;
      sdpListenSock = this->sdpListenSock;
      tcpListenSock = this->tcpListenSock;

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

      // handle incoming connection attempts
      for(size_t i=0; i < (size_t)epollRes; i++)
      {
         struct epoll_event* currentEvent = &epollEvents[i];
         Pollable* currentPollable = (Pollable*)currentEvent->data.ptr;

         //log.log(Log_DEBUG, std::string("Incoming data on FD: ") +
         //   StringTk::intToStr(pollArray[i].fd) ); // debug in

         if(currentPollable == rdmaListenSock)
            onIncomingRDMAConnection(rdmaListenSock);
         else
         if(currentPollable == tcpListenSock)
            onIncomingStandardConnection(tcpListenSock);
         else
         if(currentPollable == sdpListenSock)
            onIncomingStandardConnection(sdpListenSock);
         else
         { // unknown connection => should never happen
            log.log(Log_WARNING, "Should never happen: Ignoring event for unknown connection. "
               "FD: " + StringTk::uintToStr(currentPollable->getFD() ) );
         }
      }

   }
}

/**
 * Accept the incoming connection and add new socket to StreamListenerV2 queue.
 *
 * Note: This is for standard sockets like TCP and SDP.
 */
void ConnAcceptor::onIncomingStandardConnection(StandardSocket* sock)
{
   try
   {
      struct sockaddr_in peerAddr;
      socklen_t peerAddrLen = sizeof(peerAddr);

      StandardSocket* acceptedSock =
         (StandardSocket*)sock->accept( (struct sockaddr*)&peerAddr, &peerAddrLen);

      // (note: level Log_DEBUG to avoid spamming the log until we have log topics)
      log.log(Log_DEBUG, std::string("Accepted new connection from ") +
         Socket::endpointAddrToStr(&peerAddr) +
         std::string(" [SockFD: ") + StringTk::intToStr(acceptedSock->getFD() ) +
         std::string("]") );

      applySocketOptions(acceptedSock);

      // hand the socket over to a stream listener

      StreamListenerV2* listener = app->getStreamListenerByFD(acceptedSock->getFD() );
      StreamListenerV2::SockReturnPipeInfo returnInfo(
         StreamListenerV2::SockPipeReturn_NEWCONN, acceptedSock);

      listener->getSockReturnFD()->write(&returnInfo, sizeof(returnInfo) );

   }
   catch(SocketException& se)
   {
      log.logErr(std::string("Trying to continue after connection accept error: ") +
         se.what() );
   }
}

/**
 * Accept the incoming RDMA connection and add new socket to StreamListenerV2 queue.
 */
void ConnAcceptor::onIncomingRDMAConnection(RDMASocket* sock)
{
   auto cfg = PThread::getCurrentThreadApp()->getCommonConfig();
   /* accept the incoming connection (and loop until no more delayed events are waiting on
         this socket) */
   // (Note: RDMASockets use this internally also to handle other kindes of events) */


   /* loop: check whether this is a false alarm (i.e. an event that is handled internally by the
         socket when we call accept() ), try to accept a new connection (if
         available) and also check for further events after accept */

   while(sock->checkDelayedEvents() )
   {
      try
      {
         struct sockaddr_in peerAddr;
         socklen_t peerAddrLen = sizeof(peerAddr);

         sock->setConnectionRejectionRate(cfg->getConnectionRejectionRate());
         RDMASocket* acceptedSock =
            (RDMASocket*)sock->accept( (struct sockaddr*)&peerAddr, &peerAddrLen);

         // note: RDMASocket::accept() might return NULL (which is not an error)
         if(!acceptedSock)
         {
            log.log(Log_DEBUG, "Ignoring an internal event on the listening RDMA socket");
            continue;
         }

         acceptedSock->setTimeouts(cfg->getConnRDMATimeoutConnect(),
            cfg->getConnRDMATimeoutFlowSend(), cfg->getConnRDMATimeoutPoll());

         // (note: level Log_DEBUG to avoid spamming the log until we have log topics)
         log.log(Log_DEBUG, std::string("Accepted new RDMA connection from ") +
            Socket::endpointAddrToStr(&peerAddr) +
            std::string(" [SockFD: ") + StringTk::intToStr(acceptedSock->getFD() ) +
            std::string("]") );

         // hand the socket over to a stream listener

         StreamListenerV2* listener = app->getStreamListenerByFD(acceptedSock->getFD() );
         StreamListenerV2::SockReturnPipeInfo returnInfo(
            StreamListenerV2::SockPipeReturn_NEWCONN, acceptedSock);

         listener->getSockReturnFD()->write(&returnInfo, sizeof(returnInfo) );

      }
      catch(SocketException& se)
      {
         log.logErr(std::string("Trying to continue after RDMA connection accept error: ") +
            se.what() );
      }

   }
}

/**
 * Apply special socket options to a new incoming standard socket connection.
 */
void ConnAcceptor::applySocketOptions(StandardSocket* sock)
{
   LogContext log("ConnAcceptor (apply socket options)");

   try
   {
      sock->setTcpCork(false);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to disable TcpCork. "
         "FD: " + StringTk::uintToStr(sock->getFD() ) + ". "
         "Msg: " + std::string(e.what() ) );
   }

   try
   {
      sock->setTcpNoDelay(true);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to enable TcpNoDelay. "
         "FD: " + StringTk::uintToStr(sock->getFD() ) + ". "
         "Msg: " + std::string(e.what() ) );
   }

   try
   {
      sock->setSoKeepAlive(true);
   }
   catch(SocketException& e)
   {
      log.log(Log_NOTICE, "Failed to enable SoKeepAlive. "
         "FD: " + StringTk::uintToStr(sock->getFD() ) + ". "
         "Msg: " + std::string(e.what() ) );
   }
}


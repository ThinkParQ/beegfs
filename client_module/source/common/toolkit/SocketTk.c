#include <common/toolkit/SocketTk.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/RDMASocket.h>
#include <toolkit/ExternalHelperd.h>
#include <common/Common.h>
#include <common/toolkit/TimeTk.h>

#include <linux/in.h>
#include <linux/inet.h>
#include <linux/poll.h>
#include <linux/net.h>
#include <linux/socket.h>


struct file* SocketTkDummyFilp = NULL;


/**
 * One-time initialization of a dummy file pointer (required for polling)
 * Note: remember to call the corresponding uninit routine
 */
bool SocketTk_initOnce(void)
{
   SocketTkDummyFilp = filp_open("/dev/null", O_RDONLY, 0);
   if(IS_ERR(SocketTkDummyFilp) )
   {
      printk_fhgfs(KERN_WARNING, "Failed to open the dummy filp for polling\n");
      return false;
   }

   return true;
}

void SocketTk_uninitOnce(void)
{
   if(SocketTkDummyFilp && !IS_ERR(SocketTkDummyFilp) )
      filp_close(SocketTkDummyFilp, NULL);
}


/*
 * Synchronous I/O multiplexing for standard and RDMA sockets.
 * Note: comparable to userspace poll()
 *
 * @param pollState list of sockets and event wishes
 * @return number of socks for which interesting events are available or negative linux error code.
 *  (tvp is being set to time remaining)
 */
int SocketTk_poll(PollState* state, int timeoutMS)
{
   struct poll_wqueues stdTable;
   poll_table* stdWait = NULL; // value NULL means "don't register for waiting"
   int table_err = 0;
   int numSocksWithREvents = 0; // the return value
   Socket* socket;

   long __timeout = TimeTk_msToJiffiesSchedulable(timeoutMS);

   /* 4.19 (vanilla, not stable) had a bug in the sock_poll_wait signature. rhel 4.18 backports
    * this bug. 4.19.1 fixes it again. */
   BUILD_BUG_ON(__builtin_types_compatible_p(
            __typeof__(&sock_poll_wait),
            void (*)(struct file*, poll_table*)));

   poll_initwait(&stdTable);

   if(__timeout)
      stdWait = &stdTable.pt;

   // 1st loop: register the socks that we're waiting for and wait blocking
   // if no data is available yet
   // 2nd loop (after event or timeout): check all socks for available data
   //    note: std socks return all events, even those we haven't been waiting for
   // 3rd and futher loops: in case an uninteresting event occurred
   for( ; ; )
   {
      numSocksWithREvents = 0; // (must be inside the loop to be consistent)

      /* wait INTERRUPTIBLE if no signal is pending, otherwise the wait contributes to load.
       * users of this function should be migrated to use socket callbacks instead. */
      set_current_state(signal_pending(current) ? TASK_KILLABLE : TASK_INTERRUPTIBLE);

      // for each sock: ask for available data and register waitqueue
      list_for_each_entry(socket, &state->list, poll._list)
      {
         if(Socket_getSockType(socket) == NICADDRTYPE_RDMA)
         { // RDMA socket
            struct RDMASocket* currentRDMASock = (RDMASocket*)socket;
            bool finishPoll = (numSocksWithREvents || !__timeout);

            unsigned long mask = RDMASocket_poll(
               currentRDMASock, socket->poll._events, finishPoll);

            if(mask)
            { // interesting event occurred
               socket->poll.revents = mask; // save event mask as revents
               numSocksWithREvents++;
            }
         }
         else
         { // Standard socket
            struct socket* currentRawSock =
               StandardSocket_getRawSock( (StandardSocket*)socket);
            poll_table* currentStdWait = numSocksWithREvents ? NULL : stdWait;

            unsigned long mask = (*currentRawSock->ops->poll)(
               SocketTkDummyFilp, currentRawSock, currentStdWait);

            if(mask & (socket->poll._events | POLLERR | POLLHUP | POLLNVAL) )
            { // interesting event occurred
               socket->poll.revents = mask; // save event mask as revents
               numSocksWithREvents++;
            }

            //cond_resched(); // (commented out for latency reasons)
         }
      } // end of for_each_socket loop

      stdWait = NULL; // don't register standard socks for waiting in following loops

      // skip the waiting if we already found something
      if (numSocksWithREvents || !__timeout || fatal_signal_pending(current))
         break;

      // skip the waiting if we have an error
      if(unlikely(stdTable.error) )
      {
         table_err = stdTable.error;
         break;
      }

      // wait (and reduce remaining timeout)
      __timeout = schedule_timeout(__timeout);

   } // end of sleep loop

   __set_current_state(TASK_RUNNING);

   // cleanup loop for RDMA socks
   list_for_each_entry(socket, &state->list, poll._list)
   {
      if(Socket_getSockType(socket) == NICADDRTYPE_RDMA)
      {
         struct RDMASocket* currentRDMASock = (RDMASocket*)socket;
         RDMASocket_poll(currentRDMASock, socket->poll._events, true);
      }
   }

   // cleanup for standard socks
   poll_freewait(&stdTable);

   //printk_fhgfs(KERN_INFO, "%s:%d: return %d\n", __func__, __LINE__,
   //   table_err ? table_err : numSocksWithREvents); // debug in

   return table_err ? table_err : numSocksWithREvents;
}

/**
 * Note: Name resolution is performed by asking the helper daemon.
 */
bool SocketTk_getHostByName(struct ExternalHelperd* helperd, const char* hostname,
   struct in_addr* outIPAddr)
{
   bool retVal = true;
   char* resolveRes;

   resolveRes = ExternalHelperd_getHostByName(helperd, hostname);

   if(!resolveRes)
   { // communication with helper daemon failed => maybe hostname is an IP string
      return SocketTk_getHostByAddrStr(hostname, outIPAddr);
   }

   if(!strlen(resolveRes) )
      return false; // hostname unknown


   // we got an IP string

   retVal = SocketTk_getHostByAddrStr(resolveRes, outIPAddr);


   // clean-up
   kfree(resolveRes);

   return retVal;
}

/**
 * Note: Old kernel versions do not support validation of the IP string.
 *
 * @param outIPAddr set to INADDR_NONE if an error was detected
 * @return false for wrong input on modern kernels (>= 2.6.20), old kernels always return
 * true
 */
bool SocketTk_getHostByAddrStr(const char* hostAddr, struct in_addr* outIPAddr)
{
   if(unlikely(!in4_pton(hostAddr, strlen(hostAddr), (u8 *)outIPAddr, -1, NULL) ) )
   { // not a valid address string
      outIPAddr->s_addr = INADDR_NONE;
      return false;
   }

   return true;
}

/**
 * Note: Better use SocketTk_getHostByAddrStr(), which can also check for errors on recent kernels.
 *
 * @return INADDR_NONE if an error was detected (recent kernels only)
 */
struct in_addr SocketTk_in_aton(const char* hostAddr)
{
   struct in_addr retVal;

   // Note: retVal INADDR_NONE will be set by getHostByAddrStr()

   SocketTk_getHostByAddrStr(hostAddr, &retVal);

   return retVal;
}

/**
 * @param buf the buffer to which <IP> should be written.
 */
void SocketTk_ipaddrToStrNoAlloc(struct in_addr ipaddress, char* ipStr, size_t ipStrLen)
{
   int printRes = snprintf(ipStr, ipStrLen, "%pI4", &ipaddress);
   if(unlikely( (size_t)printRes >= ipStrLen) )
      ipStr[ipStrLen-1] = 0; // ipStrLen exceeded => zero-terminate result
}

/**
 * @return string is kalloced and needs to be kfreed
 */
char* SocketTk_ipaddrToStr(struct in_addr ipaddress)
{
   char* ipStr = os_kmalloc(SOCKETTK_IPADDRSTR_LEN);
   if (likely(ipStr != NULL))
      SocketTk_ipaddrToStrNoAlloc(ipaddress, ipStr, SOCKETTK_IPADDRSTR_LEN);
   return ipStr;
}

/**
 * @param buf the buffer to which <IP>:<port> should be written.
 */
void SocketTk_endpointAddrToStrNoAlloc(char* buf, size_t bufLen, struct in_addr ipaddress,
   unsigned short port)
{
   int printRes = snprintf(buf, bufLen, "%pI4:%u", &ipaddress, port);
   if(unlikely( (unsigned)printRes >= bufLen) )
      buf[bufLen-1] = 0; // bufLen exceeded => zero-terminate result
}

/**
 * @return string is kalloced and needs to be kfreed
 */
char* SocketTk_endpointAddrToStr(struct in_addr ipaddress, unsigned short port)
{
   char* endpointStr = os_kmalloc(SOCKETTK_ENDPOINTSTR_LEN);
   if (likely(endpointStr != NULL))
      SocketTk_endpointAddrToStrNoAlloc(endpointStr, SOCKETTK_ENDPOINTSTR_LEN, ipaddress, port);
   return endpointStr;
}



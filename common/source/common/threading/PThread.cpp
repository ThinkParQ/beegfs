#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/system/System.h>
#include "PThread.h"

#include <csignal>
#include <execinfo.h>
#include <sys/resource.h>


pthread_once_t PThread::nameOnceVar = PTHREAD_ONCE_INIT;
pthread_key_t PThread::nameKey;
pthread_once_t PThread::appOnceVar = PTHREAD_ONCE_INIT;
pthread_key_t PThread::appKey;

__thread PThread* PThread::currentThread;


DECLARE_NAMEDEXCEPTION(SignalException, "SignalException")


#define PTHREAD_BACKTRACE_ARRAY_SIZE   32


/**
 * Block signals SIGINT and SIGTERM for the calling pthread.
 *
 * Note: Blocked signals will be inherited by child threads.
 *
 * Note: This is important to make, because Linux can direct a SIGINT to any thread that has a
 * handler registered; so we need this to make sure that only the main thread (and not the worker
 * threads) receive a SIGINT, e.g. if a user presses CTRL+C.
 */
bool PThread::blockInterruptSignals()
{
   sigset_t signalMask; // signals to block

   sigemptyset (&signalMask);

   sigaddset (&signalMask, SIGINT);
   sigaddset (&signalMask, SIGTERM);

   int sigmaskRes = pthread_sigmask(SIG_BLOCK, &signalMask, NULL);

   return (sigmaskRes == 0);
}

/**
 * Unblock the signals (e.g. SIGINT) that were blocked with blockInterruptSignals().
 */
bool PThread::unblockInterruptSignals()
{
   sigset_t signalMask; // signals to unblock

   sigemptyset (&signalMask);

   sigaddset (&signalMask, SIGINT);
   sigaddset (&signalMask, SIGTERM);

   int sigmaskRes = pthread_sigmask(SIG_UNBLOCK, &signalMask, NULL);

   return (sigmaskRes == 0);
}

void PThread::registerSignalHandler()
{
   signal(SIGSEGV, PThread::signalHandler);
   signal(SIGFPE, PThread::signalHandler);
   signal(SIGBUS, PThread::signalHandler);
   signal(SIGILL, PThread::signalHandler);
   signal(SIGABRT, PThread::signalHandler);
}


void PThread::signalHandler(int sig)
{
   AbstractApp* app = PThread::getCurrentThreadApp();

   Logger* log = Logger::getLogger();
   const char* logContext = "PThread::signalHandler";

   // note: this might deadlock if the signal was thrown while the logger mutex is locked
   //    by the current thread, but that case is very unlikely

   int backtraceLength = 0;
   char** backtraceSymbols = NULL;

   void* backtraceArray[PTHREAD_BACKTRACE_ARRAY_SIZE];
   backtraceLength = backtrace(backtraceArray, PTHREAD_BACKTRACE_ARRAY_SIZE);
   backtraceSymbols = backtrace_symbols(backtraceArray, backtraceLength); // note: symbols are
      // malloc'ed and need to be freed later


   switch(sig)
   {
      case SIGSEGV:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         LOG(GENERAL, ERR, "Received a SIGSEGV. Trying to shut down...");
         log->logBacktrace(logContext, backtraceLength, backtraceSymbols);
         app->stopComponents();
         throw SignalException("Segmentation fault");
      } break;

      case SIGFPE:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         LOG(GENERAL, ERR, "Received a SIGFPE. Trying to shut down...");
         log->logBacktrace(logContext, backtraceLength, backtraceSymbols);
         app->stopComponents();
         throw SignalException("Floating point exception");
      } break;

      case SIGBUS:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         LOG(GENERAL, ERR, "Received a SIGBUS. Trying to shut down...");
         log->logBacktrace(logContext, backtraceLength, backtraceSymbols);
         app->stopComponents();
         throw SignalException("Bus error (bad memory access)");
      } break;

      case SIGILL:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         LOG(GENERAL, ERR, "Received a SIGILL. Trying to shut down...");
         log->logBacktrace(logContext, backtraceLength, backtraceSymbols);
         app->stopComponents();
         throw SignalException("Illegal instruction");
      } break;

      case SIGABRT:
      { // note: SIGABRT is special: after signal handler returns the process dies immediately
         signal(sig, SIG_DFL); // reset the handler to its default
         LOG(GENERAL, ERR, "Received a SIGABRT. Trying to shut down...");
         log->logBacktrace(logContext, backtraceLength, backtraceSymbols);
         app->stopComponents();

         PThread::sleepMS(6000); // give the other components some time to quit before we go down

         throw SignalException("Abnormal termination");
      } break;

      default:
      {
         signal(sig, SIG_DFL); // reset the handler to its default
         std::string logMsg("Received an unknown signal " + StringTk::intToStr(sig) + ". "
            "Trying to shut down...");
         log->log(1, logContext, logMsg.c_str() );
         log->logBacktrace(logContext, backtraceLength, backtraceSymbols);
         app->stopComponents();
         throw SignalException("Unknown signal");
      } break;
   }


   SAFE_FREE(backtraceSymbols);
}

void PThread::nameKeyDestructor(void* value)
{
   SAFE_DELETE_NOSET( (std::string*)value);
}

void PThread::appKeyDestructor(void* value)
{
   // nothing to be done here, because the app will be deleted by the Program class
   //SAFE_DELETE( (AbstractApp*)value);
}

/**
 * Increases or decreases current thread priority (nice level) by given offset.
 *
 * Note: This has to be called before running the thread.
*/
void PThread::setPriorityShift(int priorityShift)
{
   this->priorityShift = priorityShift;
}


/**
 * Increases or decreases current thread priority by given offset.
 *
 * Note: This has to be called before running the thread.
 *
 * @param priorityOffset positive or negative number to increase or decrease priority (relative to
 * current thread priority)
 */
bool PThread::applyPriorityShift(int priorityShift)
{
   errno = 0; // must be done because getpriority can legitimately return -1
   int currentPrio = getpriority(PRIO_PROCESS, System::getTID() );
   if( (currentPrio == -1) && errno)
   { // error occurred
      return false;
   }

   int setRes = setpriority(PRIO_PROCESS, System::getTID(), currentPrio + priorityShift);

   return (setRes == 0);
}


/**
 * Resets the shallSelfTerminate flag.
 * Useful if you want to restart a thread that was ordered to self-terminate before.
 */
void PThread::resetSelfTerminate()
{
   this->shallSelfTerminateAtomic.setZero(); // (quasi-boolean)

   selfTerminateMutex.lock(); // L O C K

   this->shallSelfTerminate = false;;

   selfTerminateMutex.unlock(); // U N L O C K
}

/**
 * Starts the new thread via pthread_create with modified attributes to bind the thread to the
 * cores of the given numa node.
 *
 * @throw PThreadCreateException, InvalidConfigException
 */
void PThread::startOnNumaNode(unsigned nodeNum)
{
   const char* logContext = "PThread (start on NUMA node)";
   static bool errorLogged = false; // to avoid log spamming on error

   // init cpuSet with cores of give numa node

   cpu_set_t cpuSet;

   int numCores = System::getNumaCoresByNode(nodeNum, &cpuSet);
   if(!numCores)
   { // something went wrong with core retrieval, so fall back to running on all cores
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING,
            "Failed to detect CPU cores for NUMA zone. Falling back to allowing all cores. "
            "Failed zone: " + StringTk::intToStr(nodeNum) );

      errorLogged = true;

      start();
      return;
   }

   // init pthread attributes with cpuSet cores

   pthread_attr_t attr;

   int attrRes = pthread_attr_init(&attr);
   if(attrRes)
      throw PThreadCreateException("pthread_attr_init failed: " +
         System::getErrString(attrRes) );

   int affinityRes = pthread_attr_setaffinity_np(&attr, sizeof(cpuSet), &cpuSet);
   if(affinityRes)
      throw PThreadCreateException("pthread_attr_setaffinity_np failed: " +
         System::getErrString(affinityRes) );

   // create and run thread

   int createRes = pthread_create(&threadID, &attr, &PThread::runStatic, this);
   if(createRes)
      throw PThreadCreateException("phtread_create (numa bind mode) failed: " +
         System::getErrString(createRes) );
}


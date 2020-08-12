#ifndef PTHREAD_H_
#define PTHREAD_H_

#include <common/system/System.h>
#include <common/threading/Atomics.h>
#include <common/threading/PThreadException.h>
#include <common/Common.h>
#include "Condition.h"
#include "PThreadCreateException.h"

#include <signal.h>
#include <sched.h>
#include <sys/prctl.h>


#define PTHREAD_KERNELTHREADNAME_DELIMITER_CHAR    '/'


// forward declaration
class AbstractApp;


/**
 * PThreads represent POSIX threads based on the normal start/stop/join mechanisms plus some special
 * stuff like thread local storage (for the App object) and the possiblity to kindly ask the thread
 * to self-terminate.
 */
class PThread
{
   public:
      virtual ~PThread()
      {
         pthread_attr_destroy(&this->threadAttr);
      }

      PThread() = delete;
      PThread(const PThread&) = delete;
      PThread(PThread&&) = delete;
      PThread& operator=(const PThread&) = delete;
      PThread& operator=(PThread&&) = delete;

      void setPriorityShift(int priorityShift);

      void startOnNumaNode(unsigned nodeNum);


      // inliners

      void join()
      {
         if (!threadID)
            throw PThreadException("Cannot join invalid thread.");

         pthread_join(threadID, NULL);
      }

      /**
       * @return true if the component terminated within the given amount of time
       */
      bool timedjoin(int timeoutMS)
      {
         if (!threadID)
            throw PThreadException("Cannot join invalid thread.");


         struct timespec joinEndtime;

         if(clock_gettime(CLOCK_REALTIME, &joinEndtime) )
            return false;

         joinEndtime.tv_sec += timeoutMS / 1000;
         joinEndtime.tv_nsec += (timeoutMS % 1000) * 1000 * 1000;

         //prevent nanosecond overflow: add whole second from nanoseconds to seconds if exists and
         //remove whole second from nanoseconds if exists
         joinEndtime.tv_sec += joinEndtime.tv_nsec / (1000*1000*1000);
         joinEndtime.tv_nsec = joinEndtime.tv_nsec % (1000*1000*1000);

         int joinRes = pthread_timedjoin_np(threadID, NULL, &joinEndtime);

         if(!joinRes)
            return true;

         if(joinRes == ETIMEDOUT)
            return false;

         throw PThreadException(System::getErrString(joinRes) );
      }

      void terminate()
      {
         if (!threadID)
            throw PThreadException("Cannot terminate invalid thread.");

         pthread_kill(threadID, SIGTERM);
      }

      void kill()
      {
         if (!threadID)
            throw PThreadException("Cannot kill invalid thread.");

         pthread_kill(threadID, SIGKILL);
      }

      /**
       * Starts the new thread by calling pthread_create() and calling its run()-method.
       */
      void start()
      {
         int createRes = pthread_create(&threadID, NULL, &PThread::runStatic, this);
         if(createRes)
            throw PThreadCreateException("phtread_create failed: " +
               System::getErrString(createRes) );
      }

      /**
       * Executes the run()-method of the PThread from the current thread without(!!)
       * creating a new pthread.
       * This is a special case, that makes a standard thread "become" a PThread.
       */
      void startInCurrentThread()
      {
         threadID = pthread_self();

         runStatic(this);
      }

      /**
       * Note: This is designed primarily for deeper_lib. It allows the lib to pretend they are
       * beegfs threads and use the routines of beegfs_common. So we init the app and the signal
       * handlers here.
       */
      static void initFakeThread(AbstractApp* app)
      {
         // store thread-local vars
         setCurrentThreadApp(app);
         registerSignalHandler();
      }

      static void sleepMS(int timeoutMS)
      {
         if(timeoutMS > 0)
            poll(NULL, 0, timeoutMS);
      }

      static void yield()
      {
         sched_yield();
      }

      void selfTerminate()
      {
         shallSelfTerminateAtomic.set(1); // (quasi-boolean)

         selfTerminateMutex.lock(); // L O C K

         shallSelfTerminate = true;
         shallSelfTerminateCond.broadcast();

         selfTerminateMutex.unlock(); // U N L O C K
      }

      /**
       * wait for self-terminate order.
       *
       * @return true if the thread should self-terminate, false if timeoutMS expired
       * without an incoming order for self-termination
       */
      bool waitForSelfTerminateOrder(const int timeoutMS)
      {
         bool shallSelfTerminate;

         selfTerminateMutex.lock(); // L O C K

         if(!this->shallSelfTerminate)
            shallSelfTerminateCond.timedwait(&this->selfTerminateMutex, timeoutMS);

         shallSelfTerminate = this->shallSelfTerminate;

         selfTerminateMutex.unlock(); // U N L O C K

         return shallSelfTerminate;
      }

      /**
       * wait infinitely for self-terminate order.
       */
      void waitForSelfTerminateOrder()
      {
         selfTerminateMutex.lock(); // L O C K

         while(!this->shallSelfTerminate)
            shallSelfTerminateCond.wait(&this->selfTerminateMutex);

         selfTerminateMutex.unlock(); // U N L O C K
      }


   protected:
      PThread(std::string name):
         addExePrefix(false)
      {
         // app will be derived from current thread

         this->threadApp = getCurrentThreadApp();

         this->threadName = name;
         this->threadID = 0;

         this->shallSelfTerminate = false;

         pthread_attr_init(&this->threadAttr);
         this->priorityShift = 0;
      }

      PThread(std::string name, AbstractApp* app):
         addExePrefix(true)
      {
         this->threadApp = app;
         setCurrentThreadApp(app);

         this->threadName = name;
         this->threadID = 0;

         this->shallSelfTerminate = false;

         pthread_attr_init(&this->threadAttr);
         this->priorityShift = 0;
      }

      virtual void run() = 0;

      static bool blockInterruptSignals();
      static bool unblockInterruptSignals();
      static void registerSignalHandler();

      void resetSelfTerminate();

      void exit()
      {
         pthread_exit(NULL);
      }


   private:
      static pthread_once_t nameOnceVar;
      static pthread_key_t nameKey;
      static pthread_once_t appOnceVar;
      static pthread_key_t appKey;

      static __thread PThread* currentThread;

      std::string threadName; // stores the name before the run-method is called
      AbstractApp* threadApp; // stores the app before the run-method is called

      bool addExePrefix;
      std::string threadKernelNameOrig; // stores the original kernel name for the thread
         // (as it was before we changed it with prctl() )

      pthread_t threadID;
      pthread_attr_t threadAttr; // for cpu affinity, priority, ...
      int priorityShift; // for system priority (nice level)

      AtomicSizeT shallSelfTerminateAtomic; /* (quasi-bool) fast atomic version to avoid mutex lock
                                               in getSelfTerminate() */
      bool shallSelfTerminate; // mutex-protected version to enable waitForSelfTerminateOrder()
      Mutex selfTerminateMutex;
      Condition shallSelfTerminateCond;

      static void signalHandler(int sig);
      static bool applyPriorityShift(int priorityShift);

      // inliners

      static void* runStatic(void* args)
      {
         // args is a pointer to the PThread instance that provides the run() method
         PThread::currentThread = (PThread*) args;

         // store thread-local vars
         setCurrentThreadName(currentThread->threadName);
         setCurrentKernelThreadName(currentThread, currentThread->threadName);
         setCurrentThreadApp(currentThread->threadApp);

         applyPriorityShift(currentThread->priorityShift);

         currentThread->run();

         return NULL;
      }

      static void nameKeyCreateOnce()
      {
         pthread_key_create(&nameKey, nameKeyDestructor);
      }

      static void nameKeyDestructor(void* value);

      static void setCurrentThreadName(std::string name)
      {
         // note: we use the once-var also to init the kernel thread name

         pthread_once(&nameOnceVar, nameKeyCreateOnce);

         std::string* nameValue = (std::string*)pthread_getspecific(nameKey);
         if(!nameValue)
         { // first-time init
            nameValue = new std::string(name);
            pthread_setspecific(nameKey, nameValue);

            return;
         }

         *nameValue = name;
      }

      static void setCurrentKernelThreadName(PThread* currentThread, std::string name)
      {
         if (!currentThread->addExePrefix)
         {
            if (name.size() > 15)
               name.resize(15);

            prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
            return;
         }

         char threadKernelName[17]; // 16 bytes limit for names (might be unterminated)

         if(currentThread->threadKernelNameOrig.empty() )
         { // original name not initialized yet
            int prctlRes = prctl(PR_GET_NAME, threadKernelName, 0, 0, 0);
            if(prctlRes)
               return;

            threadKernelName[16] = 0;

            // cut name at delimiter (because that part comes from a previous prctl() call to the
            // parent process)
            char* delimiter = strchr(threadKernelName, PTHREAD_KERNELTHREADNAME_DELIMITER_CHAR);
            if(delimiter)
               *delimiter = 0;

            currentThread->threadKernelNameOrig = threadKernelName;
         }

         if(currentThread->threadKernelNameOrig.length() <= 14)
         { // there is room for a suffix
            std::string newThreadKernelName = currentThread->threadKernelNameOrig +
               PTHREAD_KERNELTHREADNAME_DELIMITER_CHAR + name;
            prctl(PR_SET_NAME, newThreadKernelName.c_str(), 0, 0, 0); // only 16 byte names
         }
      }

      static void appKeyCreateOnce()
      {
         pthread_key_create(&appKey, appKeyDestructor);
      }

      static void appKeyDestructor(void* value);

      static void setCurrentThreadApp(AbstractApp* app)
      {
         pthread_once(&appOnceVar, appKeyCreateOnce);

         pthread_setspecific(appKey, app);
      }


   public:
      // getters & setters

      /**
       * Get name of the given thread object.
       */
      std::string getName()
      {
         return this->threadName;
      }

      /**
       * Get name of the thread calling this function.
       */
      static std::string getCurrentThreadName()
      {
         pthread_once(&nameOnceVar, nameKeyCreateOnce);

         std::string* nameValue = (std::string*)pthread_getspecific(nameKey);
         if(!nameValue)
            return "<Nameless>";

         return *nameValue;
      }

      static AbstractApp* getCurrentThreadApp()
      {
         pthread_once(&appOnceVar, appKeyCreateOnce);

         AbstractApp* appValue = (AbstractApp*)pthread_getspecific(appKey);

         return appValue;
      }

      pthread_t getID()
      {
         return threadID;
      }

      static pthread_t getCurrentThreadID()
      {
         return pthread_self();
      }

      static bool threadIDEquals(pthread_t t, pthread_t u)
      {
         return pthread_equal(t, u) != 0;
      }

      bool getSelfTerminate()
      {
         return (shallSelfTerminateAtomic.read() != 0);
      }

      static PThread* getCurrentThread() { return currentThread; }
};

#endif /*PTHREAD_H_*/

#include <common/Common.h>

#include "filesystemtk.h"
#include "threadvartk.h"


namespace threadvartk
{
   static pthread_key_t nftwFlushDiscard;
   static pthread_once_t nftwKeyOnceFlushDiscard = PTHREAD_ONCE_INIT;

   static pthread_key_t nftwFollowSymlink;
   static pthread_once_t nftwKeyOnceFollowSymlink = PTHREAD_ONCE_INIT;

   static pthread_key_t nftwNumFollowedSymlinks;
   static pthread_once_t nftwKeyOnceNumFollowedSymlinks = PTHREAD_ONCE_INIT;

   static pthread_key_t nftwDestPath;
   static pthread_once_t nftwKeyOnceDestPath = PTHREAD_ONCE_INIT;

   static pthread_key_t nftwSourcePath;
   static pthread_once_t nftwKeyOnceSourcePath = PTHREAD_ONCE_INIT;


   /**
    * constructor for the thread variable of the flush flag from the last nftw(...) call
    */
   void makeThreadKeyNftwFlushDiscard()
   {
      pthread_key_create(&threadvartk::nftwFlushDiscard, freeThreadKeyNftwFlushDiscard);
   }

   /**
    * destructor of the thread variable of the flush flag from the last nftw(...) call
    *
    * @param value pointer of the thread variable
    */
   void freeThreadKeyNftwFlushDiscard(void* value)
   {
      SAFE_DELETE_NOSET((bool*) value);
   }

   /**
    * stores the flush flags which was given to nftw(...) as a thread variable
    *
    * @param flushFlag the path to save as thread variable
    */
   void setThreadKeyNftwFlushDiscard(bool flushFlag)
   {
      pthread_once(&threadvartk::nftwKeyOnceFlushDiscard, makeThreadKeyNftwFlushDiscard);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwFlushDiscard)) != NULL)
         SAFE_DELETE_NOSET((bool*) ptr);

      ptr = new bool(flushFlag);
      pthread_setspecific(threadvartk::nftwFlushDiscard, ptr);
   }

   /**
    * reads the thread variable which contains the flush flags of the last nftw(...) call
    *
    * @return flush flags of the last nftw(...) call
    */
   bool getThreadKeyNftwFlushDiscard()
   {
      pthread_once(&threadvartk::nftwKeyOnceFlushDiscard, makeThreadKeyNftwFlushDiscard);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwFlushDiscard)) != NULL)
         return bool(*(bool*) ptr);

      return false; // not the best solution, but doesn't destroy the data
   }

   /**
    * constructor for the thread variable of the followSymlink flag from the last nftw(...) call
    */
   void makeThreadKeyNftwFollowSymlink()
   {
      pthread_key_create(&threadvartk::nftwFollowSymlink, freeThreadKeyNftwFollowSymlink);
   }

   /**
    * destructor of the thread variable of the followSymlink flag from the last nftw(...) call
    *
    * @param value pointer of the thread variable
    */
   void freeThreadKeyNftwFollowSymlink(void* value)
   {
      SAFE_DELETE_NOSET((bool*) value);
   }

   /**
    * stores the followSymlink flags which was given to nftw(...) as a thread variable
    *
    * @param followSymlink the path to save as thread variable
    */
   void setThreadKeyNftwFollowSymlink(bool followSymlinkFlag)
   {
      pthread_once(&threadvartk::nftwKeyOnceFollowSymlink, makeThreadKeyNftwFollowSymlink);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwFollowSymlink)) != NULL)
         SAFE_DELETE_NOSET((bool*) ptr);

      ptr = new bool(followSymlinkFlag);
      pthread_setspecific(threadvartk::nftwFollowSymlink, ptr);
   }

   /**
    * reads the thread variable which contains the followSymlink flags of the last nftw(...) call
    *
    * @return followSymlink flags of the last nftw(...) call
    */
   bool getThreadKeyNftwFollowSymlink()
   {
      pthread_once(&threadvartk::nftwKeyOnceFollowSymlink, makeThreadKeyNftwFollowSymlink);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwFollowSymlink)) != NULL)
         return bool(*(bool*) ptr);

      return false; // not the best solution, but doesn't destroy the data
   }

   /**
    * constructor for the thread variable of the numFollowedSymlinks counter from the last nftw(...)
    * call
    */
   void makeThreadKeyNftwNumFollowedSymlinks()
   {
      pthread_key_create(&threadvartk::nftwNumFollowedSymlinks, freeThreadKeyNftwNumFollowedSymlinks);
   }

   /**
    * destructor of the thread variable of the numFollowedSymlinks counter from the last nftw(...)
    * call
    *
    * @param value pointer of the thread variable
    */
   void freeThreadKeyNftwNumFollowedSymlinks(void* value)
   {
      SAFE_DELETE_NOSET((int*) value);
   }

   /**
    * stores the numFollowedSymlinks counter which is given to nftw(...) as a thread variable
    *
    * @param followSymlink the path to save as thread variable
    */
   void setThreadKeyNftwNumFollowedSymlinks(int numFollowedSymlinks)
   {
      pthread_once(&threadvartk::nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwNumFollowedSymlinks)) != NULL)
         SAFE_DELETE_NOSET((int*) ptr);

      ptr = new int(numFollowedSymlinks);
      pthread_setspecific(threadvartk::nftwNumFollowedSymlinks, ptr);

   #ifdef BEEGFS_DEBUG
         char buffer[20];
         char bufferMax[20];
         sprintf(buffer, "%d", numFollowedSymlinks );
         sprintf(bufferMax, "%d", filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS );
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            std::string("Number of followed symlinks: ") + buffer +
            std::string("; max allowed symlinks to follow ") + bufferMax);
   #endif
   }

   /**
    * reads the thread variable which contains the numFollowedSymlinks counter of the last nftw(...)
    * call
    *
    * @return the number of symlinks was followed
    */
   int getThreadKeyNftwNumFollowedSymlinks()
   {
      pthread_once(&threadvartk::nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwNumFollowedSymlinks)) != NULL)
      {
   #ifdef BEEGFS_DEBUG
         char buffer[20];
         char bufferMax[20];
         sprintf(buffer, "%d", int(*(int*) ptr) );
         sprintf(bufferMax, "%d", filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS );
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            std::string("Number of followed symlinks: ") + buffer +
            std::string("; max allowed symlinks to follow ") + bufferMax);
   #endif
         return int(*(int*) ptr);
      }

   #ifdef BEEGFS_DEBUG
         char buffer[20];
         char bufferMax[20];
         sprintf(buffer, "%d", 0);
         sprintf(bufferMax, "%d", filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS );
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            std::string("Number of followed symlinks: ") + buffer +
            std::string("; max allowed symlinks to follow ") + bufferMax);
   #endif

      return 0;
   }

   /**
    * resets the numFollowedSymlinks counter to zero, the counter was given to nftw(...) as a thread
    * variable, it deletes the thread variable
    */
   void resetThreadKeyNftwNumFollowedSymlinks()
   {
      pthread_once(&threadvartk::nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwNumFollowedSymlinks)) != NULL)
      {
         SAFE_DELETE_NOSET((int*) ptr);

         ptr = new int(0);
         pthread_setspecific(threadvartk::nftwNumFollowedSymlinks, ptr);
      }

   #ifdef BEEGFS_DEBUG
         char buffer[20];
         char bufferMax[20];
         sprintf(buffer, "%d", 0);
         sprintf(bufferMax, "%d", filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS );
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            std::string("Number of followed symlinks: ") + buffer +
            std::string("; max allowed symlinks to follow ") + bufferMax);
   #endif
   }

   /**
    * tests if the numFollowedSymlinks counter has reached the maximum value, the counter was given to
    * nftw(...) as a thread variable
    */
   bool testAndIncrementThreadKeyNftwNumFollowedSymlinks()
   {
      pthread_once(&threadvartk::nftwKeyOnceNumFollowedSymlinks, makeThreadKeyNftwNumFollowedSymlinks);

      void* ptr;
      int counter = 0;

      if ( (ptr = pthread_getspecific(threadvartk::nftwNumFollowedSymlinks) ) != NULL)
      {
         counter = *(int*)ptr;

         if(counter < filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS)
            SAFE_DELETE_NOSET((int*) ptr);
      }

      counter++;

      if(counter <= filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS)
      {
         ptr = new int(counter);
         pthread_setspecific(threadvartk::nftwNumFollowedSymlinks, ptr);

   #ifdef BEEGFS_DEBUG
         char buffer[20];
         char bufferMax[20];
         sprintf(buffer, "%d", counter );
         sprintf(bufferMax, "%d", filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS );
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            std::string("Number of followed symlinks: ") + buffer +
            std::string("; max allowed symlinks to follow ") + bufferMax);
   #endif

         return true;
      }

   #ifdef BEEGFS_DEBUG
         char buffer[20];
         char bufferMax[20];
         sprintf(buffer, "%d", counter );
         sprintf(bufferMax, "%d", filesystemtk::MAX_NUM_FOLLOWED_SYMLINKS );
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            std::string("Number of followed symlinks: ") + buffer +
            std::string("; max allowed symlinks to follow ") + bufferMax);
   #endif

      return false;
   }

   /**
    * constructor for the thread variable of the flush flag from the last nftw(...) call
    */
   void makeThreadKeyNftwDestPath()
   {
      pthread_key_create(&threadvartk::nftwDestPath, freeThreadKeyNftwDestPath);
   }

   /**
    * destructor of the thread variable of the flush flag from the last nftw(...) call
    *
    * @param value pointer of the thread variable
    */
   void freeThreadKeyNftwDestPath(void* value)
   {
      SAFE_DELETE_NOSET((std::string*) value);
   }

   /**
    * stores the flush flags which was given to nftw(...) as a thread variable
    *
    * @param destPath the path to save as thread variable
    */
   void setThreadKeyNftwDestPath(const char* destPath)
   {
      pthread_once(&threadvartk::nftwKeyOnceDestPath, makeThreadKeyNftwDestPath);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwDestPath)) != NULL)
         SAFE_DELETE_NOSET((std::string*) ptr);

   #ifdef BEEGFS_DEBUG
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            "New destPath: " + std::string(destPath) );
   #endif

      ptr = new std::string(destPath);
      pthread_setspecific(threadvartk::nftwDestPath, ptr);
   }

   /**
    * reads the thread variable which contains the flush flags of the last nftw(...) call
    *
    * @return flush flags of the last nftw(...) call
    */
   bool getThreadKeyNftwDestPath(std::string& outValue)
   {
      pthread_once(&threadvartk::nftwKeyOnceDestPath, makeThreadKeyNftwDestPath);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwDestPath) ) != NULL)
      {
         if(!(*(std::string*) ptr).empty() )
         {
            outValue.assign(*(std::string*) ptr);
            return true;
         }
      }

      return false;
   }

   /**
    * resets the numFollowedSymlinks counter to zero, the counter was given to nftw(...) as a thread
    * variable, it deletes the thread variable
    */
   void resetThreadKeyNftwDestPath()
   {
      pthread_once(&threadvartk::nftwKeyOnceDestPath, makeThreadKeyNftwDestPath);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwDestPath)) != NULL)
      {
         SAFE_DELETE_NOSET((int*) ptr);

         ptr = new std::string("");
         pthread_setspecific(threadvartk::nftwDestPath, ptr);
      }
   }

   /**
    * constructor for the thread variable of the flush flag from the last nftw(...) call
    */
   void makeThreadKeyNftwSourcePath()
   {
      pthread_key_create(&threadvartk::nftwSourcePath, freeThreadKeyNftwSourcePath);
   }

   /**
    * destructor of the thread variable of the flush flag from the last nftw(...) call
    *
    * @param value pointer of the thread variable
    */
   void freeThreadKeyNftwSourcePath(void* value)
   {
      SAFE_DELETE_NOSET((std::string*) value);
   }

   /**
    * stores the flush flags which was given to nftw(...) as a thread variable
    *
    * @param destPath the path to save as thread variable
    */
   void setThreadKeyNftwSourcePath(const char* sourcePath)
   {
      pthread_once(&threadvartk::nftwKeyOnceSourcePath, makeThreadKeyNftwSourcePath);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwSourcePath)) != NULL)
         SAFE_DELETE_NOSET((std::string*) ptr);

   #ifdef BEEGFS_DEBUG
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            "New destPath: " + std::string(sourcePath) );
   #endif

      ptr = new std::string(sourcePath);
      pthread_setspecific(threadvartk::nftwSourcePath, ptr);
   }

   /**
    * reads the thread variable which contains the flush flags of the last nftw(...) call
    *
    * @return flush flags of the last nftw(...) call
    */
   bool getThreadKeyNftwSourcePath(std::string& outValue)
   {
      pthread_once(&threadvartk::nftwKeyOnceSourcePath, makeThreadKeyNftwSourcePath);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwSourcePath) ) != NULL)
      {
         if(!(*(std::string*) ptr).empty() )
         {
            outValue.assign(*(std::string*) ptr);
            return true;
         }
      }

      return false;
   }

   /**
    * resets the numFollowedSymlinks counter to zero, the counter was given to nftw(...) as a thread
    * variable, it deletes the thread variable
    */
   void resetThreadKeyNftwSourcePath()
   {
      pthread_once(&threadvartk::nftwKeyOnceSourcePath, makeThreadKeyNftwSourcePath);

      void* ptr;

      if ((ptr = pthread_getspecific(threadvartk::nftwSourcePath)) != NULL)
      {
         SAFE_DELETE_NOSET((int*) ptr);

         ptr = new std::string("");
         pthread_setspecific(threadvartk::nftwSourcePath, ptr);
      }
   }

   /**
    * stores the flush flags which was given to nftw(...) as a thread variable
    *
    * @param destPath the path to save as thread variable
    */
   void setThreadKeyNftwPaths(const char* sourcePath, const char* destPath)
   {
      threadvartk::setThreadKeyNftwSourcePath(sourcePath);
      threadvartk::setThreadKeyNftwDestPath(destPath);
   }

   /**
    * stores the flush flags which was given to nftw(...) as a thread variable
    *
    * @param destPath the path to save as thread variable
    */
   bool getThreadKeyNftwPaths(std::string& sourcePath, std::string& destPath)
   {
       return (threadvartk::getThreadKeyNftwSourcePath(sourcePath) &&
          threadvartk::getThreadKeyNftwDestPath(destPath) );
   }

   /**
    * stores the flush flags which was given to nftw(...) as a thread variable
    *
    * @param destPath the path to save as thread variable
    */
   void resetThreadKeyNftwPaths()
   {
      threadvartk::resetThreadKeyNftwSourcePath();
      threadvartk::resetThreadKeyNftwDestPath();
   }
}

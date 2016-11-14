#ifndef SESSION_DEEPERCACHESESSION_H_
#define SESSION_DEEPERCACHESESSION_H_


#include <common/Common.h>
#include <deeper/deeper_cache.h>

/**
 * The session which was created during a open. The session contains all informations which are
 * needed until the file is closed.
 */
class DeeperCacheSession
{
   public:
      /**
       * Constructor
       */
      DeeperCacheSession(int fd)
      {
         this->fileDescriptor = fd;
         this->deeperOpenFlags = DEEPER_OPEN_NONE;
      };

      /**
       * Constructor
       */
      DeeperCacheSession(int fd, int deeperOpenFlags, std::string path)
      {
         this->fileDescriptor = fd;
         this->deeperOpenFlags = deeperOpenFlags;
         this->filePath = path;
      };


   private:
      int fileDescriptor;           // the file descriptor the file which was opened
      int deeperOpenFlags;          // the deeper_open_flags which was used during the open
      std::string filePath;         // the path to the file which was opened


   public:
      // getter and setter
      int getFD()
      {
         return this->fileDescriptor;
      }

      std::string getPath()
      {
         return this->filePath;
      }

      int getDeeperOpenFlags()
      {
         return this->deeperOpenFlags;
      }

      void setFD(int fd)
      {
         this->fileDescriptor = fd;
      }

      void setPath(std::string path)
      {
         this->filePath = path;
      }

      void setDeeperOpenFlags(int deeperOpenFlags)
      {
         this->deeperOpenFlags = deeperOpenFlags;
      }
};

#endif /* SESSION_DEEPERCACHESESSION_H_ */

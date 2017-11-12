#ifndef CHUNKDIR_H_
#define CHUNKDIR_H_

#include <string>

#include <common/threading/RWLock.h>

/**
 * Our inode object, but for directories only. Files are in class FileInode.
 */
class ChunkDir
{
      friend class ChunkStore;

   public:
      ChunkDir(std::string id) : id(id)
      {
      }

   protected:
      RWLock rwlock;

   private:
      std::string id; // filesystem-wide unique string
      

   public:

      // inliners

      void readLock()
      {
         this->rwlock.readLock();
      }
      
      void writeLock()
      {
         this->rwlock.writeLock();
      }
      
      void unlock()
      {
         this->rwlock.unlock();
      }

      std::string getID() const
      {
         return this->id;
      }

};

#endif /*CHUNKDIR_H_*/

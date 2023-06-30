#ifndef STORAGENODEEX_H_
#define STORAGENODEEX_H_

#include <common/nodes/Node.h>
#include <common/Common.h>
#include <common/threading/RWLockGuard.h>

struct StorageNodeDataContent
{
      bool isResponding;

      unsigned indirectWorkListSize;
      unsigned directWorkListSize;

      int64_t diskSpaceTotal;
      int64_t diskSpaceFree;
      int64_t diskRead;
      int64_t diskWrite;

      unsigned sessionCount;
      std::string hostnameid;
};

class StorageNodeEx : public Node
{
   public:
      StorageNodeEx(std::shared_ptr<Node> receivedNode);
      StorageNodeEx(std::shared_ptr<Node> receivedNode, std::shared_ptr<StorageNodeEx> oldNode);

   private:
      mutable RWLock lock;
      bool isResponding;
      std::chrono::milliseconds lastStatRequestTime{0};

   public:
      std::chrono::milliseconds getLastStatRequestTime() const
      {
         RWLockGuard safeLock(lock, SafeRWLock_READ);
         return lastStatRequestTime;
      }

      void setLastStatRequestTime(const std::chrono::milliseconds& time)
      {
         RWLockGuard safeLock(lock, SafeRWLock_READ);
         lastStatRequestTime = time;
      }

      bool getIsResponding() const
      {
         RWLockGuard safeLock(lock, SafeRWLock_READ);
         return isResponding;
      }

      void setIsResponding(bool isResponding)
      {
         RWLockGuard safeLock(lock, SafeRWLock_READ);
         this->isResponding = isResponding;
      }
};

#endif /*STORAGENODEEX_H_*/

#ifndef METANODEEX_H_
#define METANODEEX_H_

#include <common/nodes/Node.h>
#include <common/Common.h>
#include <common/threading/RWLockGuard.h>

struct MetaNodeDataContent
{
   bool isResponding;
   unsigned indirectWorkListSize;
   unsigned directWorkListSize;
   unsigned sessionCount;
   std::string hostnameid;
};

class MetaNodeEx: public Node
{
   public:
      MetaNodeEx(std::shared_ptr<Node> receivedNode);
      MetaNodeEx(std::shared_ptr<Node> receivedNode, std::shared_ptr<MetaNodeEx> oldNode);

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
         RWLockGuard safeLock(lock, SafeRWLock_WRITE);
         lastStatRequestTime = time;
      }

      bool getIsResponding() const
      {
         RWLockGuard safeLock(lock, SafeRWLock_READ);
         return isResponding;
      }

      void setIsResponding(bool isResponding)
      {
         RWLockGuard safeLock(lock, SafeRWLock_WRITE);
         this->isResponding = isResponding;
      }

};

#endif /*METANODEEX_H_*/

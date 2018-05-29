#ifndef NODESTORESERVERSEX_H_
#define NODESTORESERVERSEX_H_

#include <common/nodes/NodeStoreServers.h>


class NodeStoreServersEx : public NodeStoreServers
{
   public:
      NodeStoreServersEx(NodeType storeType);

      virtual bool addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID=NULL);
      virtual bool deleteNode(NumNodeID nodeID);

      virtual bool setRootNodeNumID(NumNodeID nodeID, bool ignoreExistingRoot,
         bool isBuddyMirrored);

      NumNodeID getLowestNodeID();

      bool loadFromFile(bool longNodeIDs);
      bool saveToFile();
      void fillStateStore();


   protected:


   private:
      Mutex storeMutex; // syncs access to the storePath file
      std::string storePath; // not thread-safe!
      bool storeDirty; // true if saved store file needs to be updated

      bool loadFromBuf(const char* buf, unsigned bufLen, bool longNodeIDs);
      ssize_t saveToBuf(boost::scoped_array<char>& buf);

      void setStoreDirty()
      {
         SafeMutexLock mutexLock(&storeMutex);
         storeDirty = true;
         mutexLock.unlock();
      }

   public:
      // getters & setters   
      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }

      bool isStoreDirty()
      {
         SafeMutexLock mutexLock(&storeMutex);
         bool retVal = this->storeDirty;
         mutexLock.unlock();

         return retVal;
      }

};

#endif /*NODESTORESERVERSEX_H_*/

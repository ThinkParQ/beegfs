#ifndef NODESTORECLIENTSEX_H_
#define NODESTORECLIENTSEX_H_

#include <common/nodes/NodeStoreClients.h>


class NodeStoreClientsEx : public NodeStoreClients
{
   public:
      NodeStoreClientsEx();

      virtual bool addOrUpdateNode(NodeHandle node);
      virtual bool deleteNode(NumNodeID nodeNumID);

      bool loadFromFile(bool longNodeIDs);
      bool saveToFile();

   private:
      Mutex storeMutex; // syncs access to the storePath file
      std::string storePath; // not thread-safe!
      bool storeDirty; // true if saved store file needs to be updated


      bool loadFromBuf(const char* buf, unsigned bufLen, bool longNodeIDs);
      ssize_t saveToBuf(boost::scoped_array<char>& buf);

   public:
      // getters & setters   
      void setStorePath(const std::string& storePath)
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

#endif /*NODESTORECLIENTSEX_H_*/

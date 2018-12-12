#ifndef NODESTORECLIENTSEX_H_
#define NODESTORECLIENTSEX_H_

#include <common/nodes/NodeStoreClients.h>


class NodeStoreClientsEx : public NodeStoreClients
{
   public:
      bool loadFromFile(bool v6Format);
      bool saveToFile();

   private:
      NumNodeID lastUsedNumID; // Note: is normally set in addOrUpdateNode

      Mutex storeMutex; // syncs access to the storePath file
      std::string storePath; // not thread-safe!

      bool loadFromBuf(const char* buf, unsigned bufLen, bool v6Format);
      ssize_t saveToBuf(boost::scoped_array<char>& buf);

      virtual NumNodeID generateID(Node& node) override;
      NumNodeID retrieveNumIDFromStringID(const std::string& nodeID) const;

   public:
      // getters & setters   
      void setStorePath(const std::string& storePath)
      {
         this->storePath = storePath;
      }
};

#endif /*NODESTORECLIENTSEX_H_*/

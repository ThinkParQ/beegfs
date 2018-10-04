#ifndef NODESTORESERVERSEX_H_
#define NODESTORESERVERSEX_H_

#include <common/nodes/NodeStoreServers.h>
#include <common/nodes/RootInfo.h>


class NodeStoreServersEx : public NodeStoreServers
{
   public:
      NodeStoreServersEx(NodeType storeType);

      NumNodeID getLowestNodeID();

      bool loadFromFile(RootInfo* root, bool v6Format);
      bool saveToFile(const RootInfo* root);
      void fillStateStore();


   protected:
      virtual NumNodeID generateID(Node& node) const override;

   private:
      Mutex storeMutex; // syncs access to the storePath file
      std::string storePath; // not thread-safe!

      bool loadFromBuf(const char* buf, unsigned bufLen, RootInfo* root, bool v6Format);
      ssize_t saveToBuf(boost::scoped_array<char>& buf, const RootInfo* root);

   public:
      // getters & setters   
      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }
};

#endif /*NODESTORESERVERSEX_H_*/

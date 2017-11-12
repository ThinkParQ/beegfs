#ifndef NODESTOREEXSERVERS_H_
#define NODESTOREEXSERVERS_H_

#include <common/nodes/NodeStoreServers.h>


class NodeStoreServersEx : public NodeStoreServers
{
   public:
      NodeStoreServersEx(NodeType storeType) throw(InvalidConfigException);

      virtual bool addOrUpdateNode(Node** node);

      bool gotRoot();
   
      
   protected:
   
      
   private:

      
   public:
      // getters & setters   

};

#endif /*NODESTOREEXSERVERS_H_*/

#ifndef MGMTNODEEX_H_
#define MGMTNODEEX_H_

#include <common/nodes/Node.h>
#include <common/Common.h>

#include <mutex>

struct MgmtdNodeDataContent
{
   bool isResponding;
};

class MgmtNodeEx : public Node
{
   public:
      MgmtNodeEx(std::string nodeID, NumNodeID nodeNumID,  unsigned short portUDP,
            unsigned short portTCP, NicAddressList& nicList);

   private:
      MgmtdNodeDataContent data;

   public:
      MgmtdNodeDataContent getContent()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->data;
      }

      void setContent(MgmtdNodeDataContent content)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->data = content;
      }
};

#endif /*MGMTNODEEX_H_*/

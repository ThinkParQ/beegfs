#ifndef MGMTNODEEX_H_
#define MGMTNODEEX_H_

/*
 * MgmtNodeEx is based on the class Node from common and extends this class by providing special
 * data structures and methods to handle admon-relevant statistics
 */

#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/nodes/Node.h>
#include <common/Common.h>

#include <mutex>

// forward declaration
class Database;


struct MgmtdNodeDataContent
{
   bool isResponding;
};


class MgmtNodeEx : public Node
{
   public:
      MgmtNodeEx(std::string nodeID, NumNodeID nodeNumID,  unsigned short portUDP,
         unsigned short portTCP, NicAddressList& nicList);

      void initialize();

   private:
      MgmtdNodeDataContent data;
      GeneralNodeInfo generalInfo;

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

      void setGeneralInformation(GeneralNodeInfo& info)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->generalInfo = info;
      }

      GeneralNodeInfo getGeneralInformation()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->generalInfo;
      }
};

#endif /*MGMTNODEEX_H_*/

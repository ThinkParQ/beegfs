#ifndef MGMTNODEEX_H_
#define MGMTNODEEX_H_

/*
 * MgmtNodeEx is based on the class Node from common and extends this class by providing special
 * data structures and methods to handle admon-relevant statistics
 */

#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/nodes/Node.h>
#include <common/threading/SafeMutexLock.h>
#include <common/Common.h>


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
         SafeMutexLock mutexLock(&mutex);
         MgmtdNodeDataContent res = this->data;
         mutexLock.unlock();
         return res;
      }

      void setContent(MgmtdNodeDataContent content)
      {
         SafeMutexLock mutexLock(&mutex);
         this->data = content;
         mutexLock.unlock();
      }

      void setGeneralInformation(GeneralNodeInfo& info)
      {
         SafeMutexLock mutexLock(&mutex);
         this->generalInfo = info;
         mutexLock.unlock();
      }

      GeneralNodeInfo getGeneralInformation()
      {
         SafeMutexLock mutexLock(&mutex);
         GeneralNodeInfo info = this->generalInfo;
         mutexLock.unlock();
         return info;
      }
};

#endif /*MGMTNODEEX_H_*/

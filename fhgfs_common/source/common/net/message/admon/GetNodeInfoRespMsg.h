#ifndef GETNODEINFORESPMSG_H_
#define GETNODEINFORESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/Common.h>

struct GeneralNodeInfo
{
      std::string cpuName;
      int32_t cpuSpeed;
      int32_t cpuCount;
      int32_t memTotal;
      int32_t memFree;
      std::string logFilePath;
};

class GetNodeInfoRespMsg: public NetMessageSerdes<GetNodeInfoRespMsg>
{
   public:
      GetNodeInfoRespMsg(const std::string& nodeID, NumNodeID nodeNumID, GeneralNodeInfo& info) :
         BaseType(NETMSGTYPE_GetNodeInfoResp)
      {
         this->info = info;
         this->nodeID = nodeID;
         this->nodeNumID = nodeNumID;
      }

      /**
       * For deserialization only
       */
      GetNodeInfoRespMsg() :
         BaseType(NETMSGTYPE_GetNodeInfoResp)
      {
      }

      TestingEqualsRes testingEquals(NetMessage* msg)
      {
         if(this->getMsgType() != msg->getMsgType())
         {
            return TestingEqualsRes_FALSE;
         }

         GetNodeInfoRespMsg* msgClone = (GetNodeInfoRespMsg*) msg;
         GeneralNodeInfo info = this->getInfo();
         GeneralNodeInfo infoClone = msgClone->getInfo();

         if (this->getNodeID() != msgClone->getNodeID())
            return TestingEqualsRes_FALSE;

         if(this->getNodeNumID() != msgClone->getNodeNumID())
            return TestingEqualsRes_FALSE;

         if(info.cpuName.compare(infoClone.cpuName) != 0)
            return TestingEqualsRes_FALSE;

         if(info.cpuSpeed != infoClone.cpuSpeed)
            return TestingEqualsRes_FALSE;

         if(info.cpuSpeed != infoClone.cpuSpeed)
            return TestingEqualsRes_FALSE;

         if(info.cpuCount != infoClone.cpuCount)
            return TestingEqualsRes_FALSE;

         if(info.memTotal != infoClone.memTotal)
            return TestingEqualsRes_FALSE;

         if(info.memFree != infoClone.memFree)
            return TestingEqualsRes_FALSE;

         if(info.logFilePath.compare(infoClone.logFilePath) != 0)
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeID
            % obj->nodeNumID
            % obj->info.cpuName
            % obj->info.cpuCount
            % obj->info.cpuSpeed
            % obj->info.memTotal
            % obj->info.memFree
            % obj->info.logFilePath;
      }

   private:
      std::string nodeID;
      NumNodeID nodeNumID;
      GeneralNodeInfo info;

   public:
      // getters & setters

      const std::string& getNodeID() const
      {
         return nodeID;
      }

      NumNodeID getNodeNumID() const
      {
         return nodeNumID;
      }

      GeneralNodeInfo getInfo() const
      {
         return info;
      }
};

#endif /*GETNODEINFORESPMSG_H_*/

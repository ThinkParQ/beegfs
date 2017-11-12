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

#ifndef GETNODEINFORESPMSG_H_
#define GETNODEINFORESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/Common.h>

struct GeneralNodeInfo
{
      std::string cpuName;
      int cpuSpeed;
      int cpuCount;
      int memTotal;
      int memFree;
      std::string logFilePath;
};

class GetNodeInfoRespMsg: public NetMessage
{
   public:

      /**
       * @param nodeID just a reference,  so do not free it as long as you use this object!
       */
      GetNodeInfoRespMsg(const char *nodeID, uint16_t nodeNumID, GeneralNodeInfo& info) :
         NetMessage(NETMSGTYPE_GetNodeInfoResp)
      {
         this->info = info;
         this->nodeID = nodeID;
         this->nodeNumID = nodeNumID;
         this->nodeIDLen = strlen(nodeID);
         this->cpuNameLen = info.cpuName.length();
         this->logFilePathLen = info.logFilePath.length();
      }

      /**
       * For deserialization only
       */
      GetNodeInfoRespMsg() :
         NetMessage(NETMSGTYPE_GetNodeInfoResp)
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

         if(strcmp(this->getNodeID(), msgClone->getNodeID()) != 0)
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

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH + Serialization::serialLenStr(nodeIDLen) + // nodeID
            Serialization::serialLenUShort() + // nodeNumID
            Serialization::serialLenStr(cpuNameLen) + // info.cpuName
            Serialization::serialLenInt() + // info.cpuCount
            Serialization::serialLenInt() + // info.cpuSpeed
            Serialization::serialLenInt() + // info.memTotal
            Serialization::serialLenInt() + // info.memFree
            Serialization::serialLenStr(logFilePathLen); // info.logFilePath
      }

   private:

      const char* nodeID;
      uint16_t nodeNumID;
      GeneralNodeInfo info;

      // for deserialization
      unsigned nodeIDLen;

      unsigned cpuNameLen;
      unsigned logFilePathLen;

   public:
      // getters & setters

      const char* getNodeID()
      {
         return nodeID;
      }

      uint16_t getNodeNumID()
      {
         return nodeNumID;
      }

      GeneralNodeInfo getInfo()
      {
         return info;
      }
};

#endif /*GETNODEINFORESPMSG_H_*/

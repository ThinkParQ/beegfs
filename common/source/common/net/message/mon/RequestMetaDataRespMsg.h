#ifndef REQUESTMETADATARESPMSG_H_
#define REQUESTMETADATARESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>


class RequestMetaDataRespMsg : public NetMessageSerdes<RequestMetaDataRespMsg>
{
   public:
      /**
       * @param hostnameid it will get the hostname of server
       * @param nicList just a reference, so do not free it as long as you use this object!
       * @param statsList just a reference, so do not free it as long as you use this object!
       */
      RequestMetaDataRespMsg(const std::string& nodeID, const std::string& hostnameid, NumNodeID nodeNumID, NicAddressList *nicList,
         bool isRoot, unsigned IndirectWorkListSize, unsigned DirectWorkListSize,
         unsigned sessionCount, HighResStatsList* statsList)
         : BaseType(NETMSGTYPE_RequestMetaDataResp)
      {
         this->nodeID = nodeID;
         this->hostnameid = hostnameid;	
         this->nodeNumID = nodeNumID;
         this->nicList = nicList;
         this->isRoot = isRoot;
         this->indirectWorkListSize = IndirectWorkListSize;
         this->directWorkListSize = DirectWorkListSize;
         this->sessionCount = sessionCount;
         this->statsList = statsList;
      }

      RequestMetaDataRespMsg() : BaseType(NETMSGTYPE_RequestMetaDataResp)
      {
         this->nicList = NULL;
         this->isRoot = 0;
         this->indirectWorkListSize = 0;
         this->directWorkListSize = 0;
         this->sessionCount = 0;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeID
            % obj->hostnameid
            % obj->nodeNumID
            % serdes::backedPtr(obj->nicList, obj->parsed.nicList)
            % obj->isRoot
            % obj->indirectWorkListSize
            % obj->directWorkListSize
            % obj->sessionCount
            % serdes::backedPtr(obj->statsList, obj->parsed.statsList);
      }

   private:
      std::string nodeID;
      std::string hostnameid;
      NumNodeID nodeNumID;
      bool isRoot;
      uint32_t indirectWorkListSize;
      uint32_t directWorkListSize;
      uint32_t sessionCount;
      NicAddressList* nicList;

      // for serialization
      HighResStatsList* statsList; // not owned by this object!

      // for deserialization
      struct {
         HighResStatsList statsList;
         NicAddressList nicList;
      } parsed;

   public:
      NicAddressList& getNicList()
      {
         return *nicList;
      }

      HighResStatsList& getStatsList()
      {
         return *statsList;
      }

      const std::string& getNodeID() const
      {
         return nodeID;
      }

      const std::string& gethostnameid() const
      {
         return hostnameid;
      }

      NumNodeID getNodeNumID() const
      {
         return nodeNumID;
      }

      NicAddressList* getNicList() const
      {
         return this->nicList;
      }

      bool getIsRoot() const
      {
         return isRoot;
      }

      unsigned getIndirectWorkListSize() const
      {
         return indirectWorkListSize;
      }

      unsigned getDirectWorkListSize() const
      {
         return directWorkListSize;
      }

      unsigned getSessionCount() const
      {
         return sessionCount;
      }
};

#endif /*REQUESTMETADATARESPMSG_H_*/

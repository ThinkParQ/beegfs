#ifndef REQUESTSTORAGEDATARESPMSG_H_
#define REQUESTSTORAGEDATARESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/toolkit/HighResolutionStats.h>


class RequestStorageDataRespMsg: public NetMessageSerdes<RequestStorageDataRespMsg>
{
   private:
      std::string nodeID;
      std::string hostnameid;
      NumNodeID nodeNumID;
      NicAddressList* nicList;
      uint32_t indirectWorkListSize;
      uint32_t directWorkListSize;
      int64_t diskSpaceTotalMiB;
      int64_t diskSpaceFreeMiB;
      uint32_t sessionCount;
      // for deserialization
      struct {
         StorageTargetInfoList storageTargets;
         HighResStatsList statsList;
         NicAddressList nicList;
      } parsed;
      // for serialization
      HighResStatsList* statsList; // not owned by this object!
      StorageTargetInfoList *storageTargets; // not owned by this object!

   public:
      /**
       * @param hostnameid it will get the hostname of server
       * @param nicList just a reference, so do not free it as long as you use this object!
       * @param statsList just a reference, so do not free it as long as you use this object!
       */
      RequestStorageDataRespMsg(const std::string& nodeID,  const std::string& hostnameid, NumNodeID nodeNumID, NicAddressList *nicList,
         unsigned indirectWorkListSize, unsigned directWorkListSize, int64_t diskSpaceTotal,
         int64_t diskSpaceFree, unsigned sessionCount, HighResStatsList* statsList,
         StorageTargetInfoList *storageTargets) :
         BaseType(NETMSGTYPE_RequestStorageDataResp)
      {
         this->nodeID = nodeID;
         this->hostnameid = hostnameid;
         this->nodeNumID = nodeNumID;
         this->nicList = nicList;
         this->indirectWorkListSize = indirectWorkListSize;
         this->directWorkListSize = directWorkListSize;
         this->diskSpaceTotalMiB = diskSpaceTotal / 1024 / 1024;
         this->diskSpaceFreeMiB = diskSpaceFree / 1024 / 1024;
         this->sessionCount = sessionCount;
         this->statsList = statsList;
         this->storageTargets = storageTargets;
      }

      RequestStorageDataRespMsg() :
         BaseType(NETMSGTYPE_RequestStorageDataResp)
      {
         this->indirectWorkListSize = 0;
         this->directWorkListSize = 0;
         this->diskSpaceTotalMiB = 0;
         this->diskSpaceFreeMiB = 0;
         this->sessionCount = 0;
         this->statsList = NULL;
         this->storageTargets = NULL;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeID
            % obj->hostnameid
            % obj->nodeNumID
            % serdes::backedPtr(obj->nicList, obj->parsed.nicList)
            % obj->indirectWorkListSize
            % obj->directWorkListSize
            % obj->diskSpaceTotalMiB
            % obj->diskSpaceFreeMiB
            % obj->sessionCount
            % serdes::backedPtr(obj->statsList, obj->parsed.statsList)
            % serdes::backedPtr(obj->storageTargets, obj->parsed.storageTargets);
      }

      NicAddressList& getNicList()
      {
         return *nicList;
      }

      HighResStatsList& getStatsList()
      {
         return *statsList;
      }

      StorageTargetInfoList& getStorageTargets()
      {
         return *storageTargets;
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

      unsigned getIndirectWorkListSize() const
      {
         return indirectWorkListSize;
      }

      unsigned getDirectWorkListSize() const
      {
         return directWorkListSize;
      }

      int64_t getDiskSpaceTotalMiB() const
      {
         return diskSpaceTotalMiB;
      }

      int64_t getDiskSpaceFreeMiB() const
      {
         return diskSpaceFreeMiB;
      }

      unsigned getSessionCount() const
      {
         return sessionCount;
      }
};

#endif /*REQUESTSTORAGEDATARESPMSG_H_*/

#ifndef SETSTORAGETARGETINFOMSG_H_
#define SETSTORAGETARGETINFOMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageTargetInfo.h>

class SetStorageTargetInfoMsg: public NetMessageSerdes<SetStorageTargetInfoMsg>
{
   public:
      SetStorageTargetInfoMsg(NodeType nodeType, StorageTargetInfoList *targetInfoList) :
         BaseType(NETMSGTYPE_SetStorageTargetInfo)
      {
         this->nodeType = nodeType;
         this->targetInfoList = targetInfoList;
      }

      SetStorageTargetInfoMsg() : BaseType(NETMSGTYPE_SetStorageTargetInfo)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeType
            % serdes::backedPtr(obj->targetInfoList, obj->parsed.targetInfoList);
      }

   private:
      StorageTargetInfoList* targetInfoList; // not owned by this object!
      int32_t nodeType;

      // for deserializaztion
      struct {
         StorageTargetInfoList targetInfoList;
      } parsed;

   public:
      NodeType getNodeType()
      {
         return (NodeType)nodeType;
      }

      const StorageTargetInfoList& getStorageTargetInfos() const
      {
         return *targetInfoList;
      }
};

#endif /*SETSTORAGETARGETINFOMSG_H_*/

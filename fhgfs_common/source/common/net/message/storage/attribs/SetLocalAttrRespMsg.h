#ifndef COMMON_SETLOCALATTRRESPMSG_H_
#define COMMON_SETLOCALATTRRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/DynamicFileAttribs.h>

#define SETLOCALATTRRESPMSG_FLAG_HAS_ATTRS 1 // message contains DynAttrs of chunk

class SetLocalAttrRespMsg : public NetMessageSerdes<SetLocalAttrRespMsg>
{
   public:
      /*
       * @param result the result of the set attr operation
       * @param dynamicAttribs The current dynamic attributes of the chunk, which are needed by
       *                       the metadata server in case of a xtime change; just a reference!
       */
      SetLocalAttrRespMsg(FhgfsOpsErr result, DynamicFileAttribs& dynamicAttribs) :
         BaseType(NETMSGTYPE_SetLocalAttrResp),
         result(result),
         filesize(dynamicAttribs.fileSize),
         numBlocks(dynamicAttribs.numBlocks),
         modificationTimeSecs(dynamicAttribs.modificationTimeSecs),
         lastAccessTimeSecs(dynamicAttribs.lastAccessTimeSecs),
         storageVersion(dynamicAttribs.storageVersion)
      {
         addMsgHeaderFeatureFlag(SETLOCALATTRRESPMSG_FLAG_HAS_ATTRS);
      }

      SetLocalAttrRespMsg(FhgfsOpsErr result) : BaseType(NETMSGTYPE_SetLocalAttrResp),
         result(result), filesize(0), numBlocks(0), modificationTimeSecs(0), lastAccessTimeSecs(0),
         storageVersion(0)
       {
         // all initialization done in list
       }

      SetLocalAttrRespMsg() : BaseType(NETMSGTYPE_SetLocalAttrResp)
      {
      }

      // getters & setters
      FhgfsOpsErr getResult() const
      {
         return result;
      }

      void getDynamicAttribs(DynamicFileAttribs* outDynamicAttribs) const
      {
         outDynamicAttribs->fileSize = filesize;
         outDynamicAttribs->lastAccessTimeSecs = lastAccessTimeSecs;
         outDynamicAttribs->modificationTimeSecs = modificationTimeSecs;
         outDynamicAttribs->numBlocks = numBlocks;
         outDynamicAttribs->storageVersion = storageVersion;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return SETLOCALATTRRESPMSG_FLAG_HAS_ATTRS;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->filesize
             % obj->numBlocks
             % obj->modificationTimeSecs
             % obj->lastAccessTimeSecs
             % obj->storageVersion
             % serdes::as<int>(obj->result);
      }

   private:
      FhgfsOpsErr result;
      int64_t filesize;
      int64_t numBlocks;
      int64_t modificationTimeSecs;
      int64_t lastAccessTimeSecs;
      uint64_t storageVersion;
};

#endif /*COMMON_SETLOCALATTRRESPMSG_H_*/

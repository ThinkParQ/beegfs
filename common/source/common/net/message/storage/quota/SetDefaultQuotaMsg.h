#ifndef COMMON_SETDEFAULTQUOTAMSG_H_
#define COMMON_SETDEFAULTQUOTAMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaData.h>

class SetDefaultQuotaMsg : public NetMessageSerdes<SetDefaultQuotaMsg>
{
   public:
      SetDefaultQuotaMsg() : BaseType(NETMSGTYPE_SetDefaultQuota) {};

      SetDefaultQuotaMsg(StoragePoolId storagePoolId, QuotaDataType newType, uint64_t newSize,
                         uint64_t newInodes):
            BaseType(NETMSGTYPE_SetDefaultQuota), storagePoolId(storagePoolId), type(newType),
            size(newSize), inodes(newInodes)
      {};

      virtual ~SetDefaultQuotaMsg() {};

   protected:
      StoragePoolId storagePoolId;
      int type;                    // enum QuotaDataType
      uint64_t size;               // defined size limit or used size (bytes)
      uint64_t inodes;             // defined inodes limit or used inodes (counter)

   public:
      //getter and setter

      uint64_t getSize() const
      {
         return size;
      }

      uint64_t getInodes() const
      {
         return inodes;
      }

      QuotaDataType getType() const
      {
         return (QuotaDataType)type;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->storagePoolId
            % obj->size
            % obj->inodes
            % obj->type;
      }
};

#endif /* COMMON_SETDEFAULTQUOTAMSG_H_ */

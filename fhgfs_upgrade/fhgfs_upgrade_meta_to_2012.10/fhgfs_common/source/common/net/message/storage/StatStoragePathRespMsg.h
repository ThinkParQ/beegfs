#ifndef STATSTORAGEPATHMSGRESP_H_
#define STATSTORAGEPATHMSGRESP_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class StatStoragePathRespMsg : public NetMessage
{
   public:
      /*
       * @param result FhgfsOpsErr
       */
      StatStoragePathRespMsg(int result, int64_t sizeTotal, int64_t sizeFree,
         int64_t inodesTotal, int64_t inodesFree) :
         NetMessage(NETMSGTYPE_StatStoragePathResp)
      {
         this->result = result;
         this->sizeTotal = sizeTotal;
         this->sizeFree = sizeFree;
         this->inodesTotal = inodesTotal;
         this->inodesFree = inodesFree;
      }

      StatStoragePathRespMsg() : NetMessage(NETMSGTYPE_StatStoragePathResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            Serialization::serialLenInt64() + // sizeTotal
            Serialization::serialLenInt64() + // sizeFree
            Serialization::serialLenInt64() + // inodesTotal
            Serialization::serialLenInt64(); // inodesFree
      }
      
   private:
      int result;
      int64_t sizeTotal;
      int64_t sizeFree;
      int64_t inodesTotal;
      int64_t inodesFree;

   public:
      // getters & setters
      int getResult()
      {
         return result;
      }

      int64_t getSizeTotal()
      {
         return sizeTotal;
      }

      int64_t getSizeFree()
      {
         return sizeFree;
      }

      int64_t getInodesTotal()
      {
         return inodesTotal;
      }

      int64_t getInodesFree()
      {
         return inodesFree;
      }
};

#endif /*STATSTORAGEPATHMSGRESP_H_*/

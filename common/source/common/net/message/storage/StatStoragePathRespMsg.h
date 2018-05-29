#ifndef STATSTORAGEPATHMSGRESP_H_
#define STATSTORAGEPATHMSGRESP_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class StatStoragePathRespMsg : public NetMessageSerdes<StatStoragePathRespMsg>
{
   public:
      /*
       * @param result FhgfsOpsErr
       */
      StatStoragePathRespMsg(int result, int64_t sizeTotal, int64_t sizeFree,
         int64_t inodesTotal, int64_t inodesFree) :
         BaseType(NETMSGTYPE_StatStoragePathResp)
      {
         this->result = result;
         this->sizeTotal = sizeTotal;
         this->sizeFree = sizeFree;
         this->inodesTotal = inodesTotal;
         this->inodesFree = inodesFree;
      }

      StatStoragePathRespMsg() : BaseType(NETMSGTYPE_StatStoragePathResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->sizeTotal
            % obj->sizeFree
            % obj->inodesTotal
            % obj->inodesFree;
      }

   private:
      int32_t result;
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

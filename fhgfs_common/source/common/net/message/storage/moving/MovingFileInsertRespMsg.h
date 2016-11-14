#ifndef MOVINGFILEINSERTRESPMSG_H_
#define MOVINGFILEINSERTRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>


class MovingFileInsertRespMsg : public NetMessageSerdes<MovingFileInsertRespMsg>
{
   public:
      /**
       * @param inodeBuf inode of overwritten file, might be NULL if none (and inodeBufLen must also
       *    be 0 in that case).
       */
      MovingFileInsertRespMsg(FhgfsOpsErr result, unsigned inodeBufLen, char* inodeBuf) :
         BaseType(NETMSGTYPE_MovingFileInsertResp)
      {
         this->result      = result;
         this->inodeBufLen = inodeBufLen;
         this->inodeBuf    = inodeBuf;
      }

      /**
       * Constructor for deserialization only.
       */
      MovingFileInsertRespMsg() : BaseType(NETMSGTYPE_MovingFileInsertResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->inodeBufLen
            % serdes::rawBlock(obj->inodeBuf, obj->inodeBufLen);
      }

   private:
      int32_t result;
      uint32_t inodeBufLen; // might be 0 if there was no file overwritten
      const char* inodeBuf; // might be NULL if there was no file overwritten


   public:
      // getters & setters

      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)this->result;
      }
      
      unsigned getInodeBufLen() const
      {
         return this->inodeBufLen;
      }

      const char* getInodeBuf() const
      {
         return this->inodeBuf;
      }

};

#endif /*MOVINGFILEINSERTRESPMSG_H_*/

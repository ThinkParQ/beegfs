#ifndef GETXATTRRESPMSG_H_
#define GETXATTRRESPMSG_H_

#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class GetXAttrRespMsg : public NetMessageSerdes<GetXAttrRespMsg>
{
   public:
      GetXAttrRespMsg(const CharVector& value, int size, int returnCode)
         : BaseType(NETMSGTYPE_GetXAttrResp),
           value(value), size(size), returnCode(returnCode)
      {
      }

      /**
       * For deserialization only.
       */
      GetXAttrRespMsg() : BaseType(NETMSGTYPE_GetXAttrResp) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->value
            % obj->size
            % obj->returnCode;
      }

   private:
      CharVector value;
      int32_t size;
      int32_t returnCode;

   public:
      // getters & setters
      const CharVector& getValue() const
      {
         return this->value;
      }

      int getReturnCode() const
      {
         return this->returnCode;
      }

      int getSize() const
      {
         return this->size;
      }
};

#endif /*GETXATTRRESPMSG_H_*/

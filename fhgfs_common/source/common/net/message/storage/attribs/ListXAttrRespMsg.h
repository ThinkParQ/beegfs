#ifndef LISTXATTRRESPMSG_H_
#define LISTXATTRRESPMSG_H_

#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class ListXAttrRespMsg : public NetMessageSerdes<ListXAttrRespMsg>
{
   public:
      ListXAttrRespMsg(const StringVector& value, int size, int returnCode)
         : BaseType(NETMSGTYPE_ListXAttrResp),
           value(value), size(size), returnCode(returnCode)
      {
      }

      /**
       * For deserialization only.
       */
      ListXAttrRespMsg() : BaseType(NETMSGTYPE_ListXAttrResp) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->value
            % obj->size
            % obj->returnCode;
      }

   private:
      StringVector value;
      int32_t size;
      int32_t returnCode;


   public:
      // getters & setters
      const StringVector& getValue() const
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

#endif /*LISTXATTRRESPMSG_H_*/

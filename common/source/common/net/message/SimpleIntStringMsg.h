#ifndef SIMPLEINTSTRINGMSG_H_
#define SIMPLEINTSTRINGMSG_H_

#include "NetMessage.h"

/**
 * Simple message containing an integer value and a string (e.g. int error code and human-readable
 * explantion with more details as string).
 */
class SimpleIntStringMsg : public NetMessageSerdes<SimpleIntStringMsg>
{
   protected:
      /**
       * @param strValue just a reference
       */
      SimpleIntStringMsg(unsigned short msgType, int intValue, std::string strValue) :
         BaseType(msgType),
         intValue(intValue), strValue(std::move(strValue))
      { }

      /**
       * For deserialization only!
       */
      SimpleIntStringMsg(unsigned short msgType) : BaseType(msgType)
      {
      }

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->intValue
            % obj->strValue;
      }

   private:
      int32_t intValue;

      std::string strValue;

   public:
      int getIntValue() const { return intValue; }

      const std::string& getStrValue() const { return strValue; }
};


#endif /* SIMPLEINTSTRINGMSG_H_ */

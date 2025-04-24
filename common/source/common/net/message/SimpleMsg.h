#pragma once

#include "NetMessage.h"

/**
 * Simple messages are defined by the header (resp. the msgType) only and
 * require no additional data
 */
class SimpleMsg : public NetMessage
{
   public:
      SimpleMsg(unsigned short msgType) : NetMessage(msgType)
      {
      }

   protected:
      virtual void serializePayload(Serializer& ser) const
      {
         // nothing to be done here for simple messages
      }

      virtual bool deserializePayload(const char* buf, size_t bufLen)
      {
         // nothing to be done here for simple messages
         return true;
      }
};



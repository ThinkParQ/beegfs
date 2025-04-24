#pragma once

#include <common/net/message/SimpleIntMsg.h>

class SetLastBuddyCommOverrideRespMsg : public SimpleIntMsg
{
   public:

      /**
       * @param result
       */
      SetLastBuddyCommOverrideRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_SetLastBuddyCommOverrideResp, result)
      {
      }

      /**
       * For deserialization only!
       */
      SetLastBuddyCommOverrideRespMsg() : SimpleIntMsg(NETMSGTYPE_SetLastBuddyCommOverrideResp) {}

   public:
      // getters & setters
      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)getValue();
      }

};


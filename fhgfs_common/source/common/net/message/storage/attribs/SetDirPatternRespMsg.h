#ifndef SETDIRPATTERNRESPMSG_H_
#define SETDIRPATTERNRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>


class SetDirPatternRespMsg : public SimpleIntMsg
{
   public:
      SetDirPatternRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetDirPatternResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetDirPatternRespMsg() : SimpleIntMsg(NETMSGTYPE_SetDirPatternResp)
      {
      }


   private:

   public:
      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};


#endif /*SETDIRPATTERNRESPMSG_H_*/

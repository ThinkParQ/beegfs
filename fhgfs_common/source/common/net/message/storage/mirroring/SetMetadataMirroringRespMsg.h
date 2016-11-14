#ifndef SETMETADATAMIRRORINGRESPMSG_H_
#define SETMETADATAMIRRORINGRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>


class SetMetadataMirroringRespMsg : public SimpleIntMsg
{
   public:
      SetMetadataMirroringRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_SetMetadataMirroringResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetMetadataMirroringRespMsg() : SimpleIntMsg(NETMSGTYPE_SetMetadataMirroringResp)
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


#endif /*SETMETADATAMIRRORINGRESPMSG_H_*/

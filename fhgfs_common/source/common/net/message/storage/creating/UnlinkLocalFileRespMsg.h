#ifndef UNLINKLOCALFILERESPMSG_H_
#define UNLINKLOCALFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class UnlinkLocalFileRespMsg : public SimpleIntMsg
{
   public:
      UnlinkLocalFileRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_UnlinkLocalFileResp, result)
      {
      }

      UnlinkLocalFileRespMsg() : SimpleIntMsg(NETMSGTYPE_UnlinkLocalFileResp)
      {
      }


   private:


   public:
      // inliners
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /*UNLINKLOCALFILERESPMSG_H_*/

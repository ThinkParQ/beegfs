#ifndef UNLINKLOCALFILEINODERESPMSG_H_
#define UNLINKLOCALFILEINODERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class UnlinkLocalFileInodeRespMsg : public SimpleIntMsg
{
   public:
      UnlinkLocalFileInodeRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_UnlinkLocalFileInodeResp, result)
      {
      }

      /**
       * Constructor for deserialization only.
       */
      UnlinkLocalFileInodeRespMsg() : SimpleIntMsg(NETMSGTYPE_UnlinkLocalFileInodeResp)
      {
      }

      // getters & setters
      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /* UNLINKLOCALFILEINODERESPMSG_H_ */

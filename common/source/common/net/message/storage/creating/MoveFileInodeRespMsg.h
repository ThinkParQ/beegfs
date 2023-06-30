#ifndef MOVEFILEINODERESPMSG_H_
#define MOVEFILEINODERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class MoveFileInodeRespMsg : public SimpleIntMsg
{
   public:
      MoveFileInodeRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_MoveFileInodeResp, result)
      {
      }

      /**
       * Constructor for deserialization only
       */
      MoveFileInodeRespMsg() : SimpleIntMsg(NETMSGTYPE_MoveFileInodeResp)
      {
      }

      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /* MOVEFILEINODERESPMSG_H_ */

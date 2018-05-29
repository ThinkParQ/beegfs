#ifndef RMDIRENTRYRESPMSG_H_
#define RMDIRENTRYRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class RmDirEntryRespMsg : public SimpleIntMsg
{
   public:
      RmDirEntryRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RmDirEntryResp, result)
      {
      }

      RmDirEntryRespMsg() : SimpleIntMsg(NETMSGTYPE_RmDirEntryResp)
      {
      }
};

#endif /* RMDIRENTRYRESPMSG_H_ */

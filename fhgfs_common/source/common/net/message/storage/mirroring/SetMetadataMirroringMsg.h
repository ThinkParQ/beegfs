#ifndef SETMETADATAMIRRORINGMSG_H_
#define SETMETADATAMIRRORINGMSG_H_

#include <common/net/message/SimpleMsg.h>
#include <common/storage/EntryInfo.h>
#include <common/Common.h>


/**
 * Enable metadata mirroring on the root directory.
 */
class SetMetadataMirroringMsg : public SimpleMsg
{
   public:
      SetMetadataMirroringMsg() : SimpleMsg(NETMSGTYPE_SetMetadataMirroring)
      {
      }
};


#endif /*SETMETADATAMIRRORINGMSG_H_*/

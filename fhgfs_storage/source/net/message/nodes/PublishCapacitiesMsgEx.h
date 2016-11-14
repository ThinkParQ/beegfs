#ifndef PUBLISHCAPACITIESMSGEX_H_
#define PUBLISHCAPACITIESMSGEX_H_

#include <common/net/message/nodes/PublishCapacitiesMsg.h>


class PublishCapacitiesMsgEx : public PublishCapacitiesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* PUBLISHCAPACITIESMSGEX_H_*/

#ifndef PUBLISHCAPACITIESMSG_H_
#define PUBLISHCAPACITIESMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/Common.h>

/**
 * This message notifies the server that the mgmtd needs its free capacity information. It is
 * typically sent to the storage and metadata servers when the mgmtd starts up and does not have any
 * capacity information yet. The storage/metadata servers should then publish their free capacity
 * info on the next InternodeSyncer run.
 */
class PublishCapacitiesMsg : public AcknowledgeableMsgSerdes<PublishCapacitiesMsg>
{
   public:
      /**
       * Default constructor for serialization and deserialization.
       */
      PublishCapacitiesMsg() : BaseType(NETMSGTYPE_PublishCapacities)
      { }
};

#endif /*PUBLISHCAPACITIESMSG_H_*/

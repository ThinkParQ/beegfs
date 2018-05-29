#ifndef MIRRORMETADATAMSG_H_
#define MIRRORMETADATAMSG_H_

#include <common/net/message/NetMessage.h>


class MirrorerTask; // forward declaration
typedef std::list<MirrorerTask*> MirrorerTaskList; // forward declaration


/**
 * Note: This msg is abstract. It does not implement (de)serialization, because we would need to
 * move class MirrorerTask to fhgfs_common for that, which is difficult, because MirrorerTask.cpp
 * needs data from fhgfs_meta (e.g. config values). So fhgfs_meta implements (de)serialization
 * in its derived class MirrorMetadataMsgEx. That is ok as long as no other services need to
 * send/recv this msg.
 */
class MirrorMetadataMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;

   public:

   protected:
      /**
       * @param taskList just a reference, so do not free it as long as you use this object
       * @param taskListNumElems number of all elements in list
       * @param taskListSerialLen serial length of all elements in list
       */
      MirrorMetadataMsg(MirrorerTaskList* taskList, unsigned taskListNumElems,
         unsigned taskListSerialLen) : NetMessage(NETMSGTYPE_MirrorMetadata)
      {
         this->taskList = taskList;
         this->taskListNumElems = taskListNumElems;
         this->taskListSerialLen = taskListSerialLen;
      }


      /**
       * For deserialization only
       */
      MirrorMetadataMsg() : NetMessage(NETMSGTYPE_MirrorMetadata) {}

      // for serialization
      MirrorerTaskList* taskList; // not owned by this object
      unsigned taskListNumElems; // number of elements in list
      unsigned taskListSerialLen; // serial length of all elements in list
};


#endif /* MIRRORMETADATAMSG_H_ */

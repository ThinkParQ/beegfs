#ifndef MIRRORMETADATARESPMSG_H_
#define MIRRORMETADATARESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>


class MirrorMetadataRespMsg : public SimpleIntMsg
{
   public:
      MirrorMetadataRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_MirrorMetadataResp, result)
      {
      }

      /**
       * For deserialization only
       */
      MirrorMetadataRespMsg() : SimpleIntMsg(NETMSGTYPE_MirrorMetadataResp)
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


#endif /* MIRRORMETADATARESPMSG_H_ */

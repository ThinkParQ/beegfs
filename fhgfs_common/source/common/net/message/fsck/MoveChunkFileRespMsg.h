#ifndef MOVECHUNKFILERESPMESG_H
#define MOVECHUNKFILERESPMESG_H

#include <common/net/message/SimpleIntMsg.h>

class MoveChunkFileRespMsg : public SimpleIntMsg
{
   public:
      MoveChunkFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_MoveChunkFileResp, result)
      {
      }

      /**
       * For deserialization only
       */
      MoveChunkFileRespMsg() : SimpleIntMsg(NETMSGTYPE_MoveChunkFileResp)
      {
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         MoveChunkFileRespMsg* msgIn = (MoveChunkFileRespMsg*) msg;

         if ( this->getValue() != msgIn->getValue() )
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }
};


#endif /*MOVECHUNKFILERESPMESG_H*/

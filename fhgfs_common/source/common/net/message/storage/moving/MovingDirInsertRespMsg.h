#ifndef MOVINGDIRINSERTRESPMSG_H_
#define MOVINGDIRINSERTRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class MovingDirInsertRespMsg : public SimpleIntMsg
{
   public:
      MovingDirInsertRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_MovingDirInsertResp, result)
      {
      }

      /**
       * Constructor for deserialization only.
       */
      MovingDirInsertRespMsg() : SimpleIntMsg(NETMSGTYPE_MovingDirInsertResp)
      {
      }


   private:


   public:
      // getters & setters

      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)getValue();
      }
};


#endif /*MOVINGDIRINSERTRESPMSG_H_*/

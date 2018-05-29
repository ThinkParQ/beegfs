#ifndef DUMMYMSG_H_
#define DUMMYMSG_H_

#include <common/net/message/SimpleMsg.h>

class DummyMsg : public SimpleMsg
{
   public:
      DummyMsg() : SimpleMsg(NETMSGTYPE_Dummy)
      {
      }
};


#endif /* DUMMYMSG_H_ */

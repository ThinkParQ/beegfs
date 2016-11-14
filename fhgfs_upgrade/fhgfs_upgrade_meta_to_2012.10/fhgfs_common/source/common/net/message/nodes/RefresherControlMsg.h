#ifndef REFRESHERCONTROLMSG_H_
#define REFRESHERCONTROLMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/Common.h>


/**
 * Control argument for the RefresherControlMsg.
 */
enum RefresherControlType
{
   RefresherControl_STATUS=0, /* query status information */
   RefresherControl_START=1, /* start refreshing */
   RefresherControl_STOP=2, /* stop refreshing */
};


class RefresherControlMsg : public SimpleIntMsg
{
   public:
      RefresherControlMsg(RefresherControlType controlCommand) :
         SimpleIntMsg(NETMSGTYPE_RefresherControl, controlCommand)
      {
      }


   protected:
      /**
       * This constructor is only for deserialization.
       */
      RefresherControlMsg() : SimpleIntMsg(NETMSGTYPE_RefresherControl)
      {
      }


   public:
      // getters & setters
      RefresherControlType getControlCommand()
      {
         return (RefresherControlType)getValue();
      }
};


#endif /* REFRESHERCONTROLMSG_H_ */

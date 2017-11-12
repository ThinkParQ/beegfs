#ifndef SETROOTMDSMSG_H
#define SETROOTMDSMSG_H

#include <common/net/message/SimpleUInt16Msg.h>

class SetRootMDSMsg : public SimpleUInt16Msg
{
   public:
      SetRootMDSMsg(uint16_t newRootMDS) : SimpleUInt16Msg(NETMSGTYPE_SetRootMDS, newRootMDS)
      {
      }


   protected:
      SetRootMDSMsg() : SimpleUInt16Msg(NETMSGTYPE_SetRootMDS)
      {
      }


   public:
      // getters & setters

      uint16_t getRootNumID()
      {
         return getValue();
      }
};


#endif /*SETROOTMDSMSG_H*/

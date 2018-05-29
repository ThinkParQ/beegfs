#ifndef SETROOTNODEIDMSG_H
#define SETROOTNODEIDMSG_H

// this message is currently only used when meta buddy mirroring is turned on to set the buddy
// group id as rootNodeID in mgmtd

#include <common/net/message/NetMessage.h>

class SetRootNodeIDMsg : public NetMessageSerdes<SetRootNodeIDMsg>
{
   public:
      SetRootNodeIDMsg(uint16_t newRootNodeID, bool rootIsBuddyMirrored) :
         BaseType(NETMSGTYPE_SetRootNodeID),
         newRootNodeID(newRootNodeID),
         rootIsBuddyMirrored(rootIsBuddyMirrored)
      {
         // all initialization done in initialization list
      }

      SetRootNodeIDMsg() : BaseType(NETMSGTYPE_SetRootNodeID)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->newRootNodeID
            % obj->rootIsBuddyMirrored;
      }

   private:
      uint16_t newRootNodeID;
      bool rootIsBuddyMirrored;

   public:
      // getters & setters
      uint16_t getRootNodeID()
      {
         return newRootNodeID;
      }

      bool getRootIsBuddyMirrored()
      {
         return rootIsBuddyMirrored;
      }
};


#endif /*SETROOTNODEIDMSG_H*/

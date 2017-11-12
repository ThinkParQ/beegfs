#ifndef SETMIRRORBUDDYGROUPRESPMSG_H_
#define SETMIRRORBUDDYGROUPRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>


class SetMirrorBuddyGroupRespMsg : public NetMessageSerdes<SetMirrorBuddyGroupRespMsg>
{
   public:
      /**
       * @param result FhgfsOpsErr
       *
       * NOTE: result may be one of the following:
       * - FhgfsOpsErr_SUCCESS => new group added
       * - FhgfsOpsErr_EXISTS => group already exists and was updated
       * - FhgfsOpsErr_INUSE => group could not be updated because target is already in use
       *
       * @param groupID the created ID of the group, if the group was new; 0 otherwise
       */
      SetMirrorBuddyGroupRespMsg(FhgfsOpsErr result, uint16_t groupID = 0) :
         BaseType(NETMSGTYPE_SetMirrorBuddyGroupResp), result(result), groupID(groupID)
      {
      }

      SetMirrorBuddyGroupRespMsg() :
         BaseType(NETMSGTYPE_SetMirrorBuddyGroupResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         uint16_t padding = 0; // PADDING

         ctx
            % serdes::as<int32_t>(obj->result)
            % obj->groupID
            % padding;
      }

   private:
      FhgfsOpsErr result;
      uint16_t groupID;

   public:
      FhgfsOpsErr getResult() const { return result; }

      uint16_t getBuddyGroupID() const { return groupID; }
};


#endif /* SETMIRRORBUDDYGROUPRESPMSG_H_ */

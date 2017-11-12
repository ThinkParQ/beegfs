#ifndef COMMON_MODIFYSTORAGEPOOLMSG_H_
#define COMMON_MODIFYSTORAGEPOOLMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StoragePoolId.h>

class ModifyStoragePoolMsg : public NetMessageSerdes<ModifyStoragePoolMsg>
{
   public:
      // message flags
      struct MsgFlags
      {
         static const unsigned HAS_NEWDESCRIPTION = 1;  /*if the message contains a new name*/
         static const unsigned HAS_ADDTARGETS     = 2;  /*message contains targets to add*/
         static const unsigned HAS_RMTARGETS      = 4;  /*message contains targets to remove*/
         static const unsigned HAS_ADDBUDDYGROUPS = 8;  /*message contains buddy groups to add*/
         static const unsigned HAS_RMBUDDYGROUPS  = 16; /*message contains buddy groups to remove*/
      };

      ModifyStoragePoolMsg(StoragePoolId poolId, const UInt16Vector* addTargets,
         const UInt16Vector* rmTargets, const UInt16Vector* addBuddyGroups,
         const UInt16Vector* rmBuddyGroups, const std::string* newDescription) :
         BaseType(NETMSGTYPE_ModifyStoragePool), poolId(poolId), addTargets(addTargets),
         rmTargets(rmTargets), addBuddyGroups(addBuddyGroups),
         rmBuddyGroups(rmBuddyGroups), newDescription(newDescription)
      {
         if (addTargets && !addTargets->empty())
            addMsgHeaderFeatureFlag(MsgFlags::HAS_ADDTARGETS);

         if (rmTargets && !rmTargets->empty())
            addMsgHeaderFeatureFlag(MsgFlags::HAS_RMTARGETS);

         if (addBuddyGroups && !addBuddyGroups->empty())
            addMsgHeaderFeatureFlag(MsgFlags::HAS_ADDBUDDYGROUPS);

         if (rmBuddyGroups && !rmBuddyGroups->empty())
            addMsgHeaderFeatureFlag(MsgFlags::HAS_RMBUDDYGROUPS);

         if (newDescription && !newDescription->empty())
            addMsgHeaderFeatureFlag(MsgFlags::HAS_NEWDESCRIPTION);
      }

      /**
       * For deserialization only
       */
      ModifyStoragePoolMsg():
         BaseType(NETMSGTYPE_ModifyStoragePool), addTargets(nullptr), rmTargets(nullptr),
         addBuddyGroups(nullptr), rmBuddyGroups(nullptr), newDescription(nullptr)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->poolId;

         if (obj->isMsgHeaderFeatureFlagSet(MsgFlags::HAS_NEWDESCRIPTION))
            ctx % serdes::backedPtr(obj->newDescription, obj->parsed.newDescription);

         if (obj->isMsgHeaderFeatureFlagSet(MsgFlags::HAS_ADDTARGETS))
            ctx % serdes::backedPtr(obj->addTargets, obj->parsed.addTargets);

         if (obj->isMsgHeaderFeatureFlagSet(MsgFlags::HAS_RMTARGETS))
            ctx % serdes::backedPtr(obj->rmTargets, obj->parsed.rmTargets);

         if (obj->isMsgHeaderFeatureFlagSet(MsgFlags::HAS_ADDBUDDYGROUPS))
            ctx % serdes::backedPtr(obj->addBuddyGroups, obj->parsed.addBuddyGroups);

         if (obj->isMsgHeaderFeatureFlagSet(MsgFlags::HAS_RMBUDDYGROUPS))
            ctx % serdes::backedPtr(obj->rmBuddyGroups, obj->parsed.rmBuddyGroups);
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return MsgFlags::HAS_NEWDESCRIPTION | MsgFlags::HAS_ADDTARGETS | MsgFlags::HAS_RMTARGETS |
                MsgFlags::HAS_ADDBUDDYGROUPS | MsgFlags::HAS_RMBUDDYGROUPS;
      }

   protected:
      StoragePoolId poolId;
      const UInt16Vector* addTargets; // not owned by this object
      const UInt16Vector* rmTargets; // not owned by this object
      const UInt16Vector* addBuddyGroups; // not owned by this object
      const UInt16Vector* rmBuddyGroups; // not owned by this object
      const std::string* newDescription; // not owned by this object

      // for deserialization
      struct
      {
         UInt16Vector addTargets;
         UInt16Vector rmTargets;
         UInt16Vector addBuddyGroups;
         UInt16Vector rmBuddyGroups;
         std::string newDescription;
      } parsed;
};


#endif /*COMMON_MODIFYSTORAGEPOOLMSG_H_*/

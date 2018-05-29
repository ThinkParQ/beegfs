#ifndef UPDATEDIRPARENTMSG_H_
#define UPDATEDIRPARENTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>

class UpdateDirParentMsg : public MirroredMessageBase<UpdateDirParentMsg>
{
   public:

      /**
       * @param entryInfoPtr just a reference, so do not free it as long as you use this object!
       */
      UpdateDirParentMsg(EntryInfo* entryInfoPtr, NumNodeID parentOwnerNodeID) :
         BaseType(NETMSGTYPE_UpdateDirParent)
      {
         this->entryInfoPtr      = entryInfoPtr;
         this->parentOwnerNodeID = parentOwnerNodeID;
      }

      UpdateDirParentMsg() : BaseType(NETMSGTYPE_UpdateDirParent) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->parentOwnerNodeID;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->dirTimestamps;
      }

   private:
      NumNodeID parentOwnerNodeID;

      // for serialization
      EntryInfo *entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

   protected:
      MirroredTimestamps dirTimestamps;

   public:
      // getters & setters
      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      NumNodeID getParentNodeID()
      {
         return parentOwnerNodeID;
      }

      bool supportsMirroring() const { return true; }
};



#endif // UPDATEDIRPARENTMSG_H_

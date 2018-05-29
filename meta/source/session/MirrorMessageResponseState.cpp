#include "MirrorMessageResponseState.h"

#include <net/message/session/AckNotifyMsgEx.h>
#include <net/message/session/BumpFileVersionMsgEx.h>
#include <net/message/session/locking/FLockEntryMsgEx.h>
#include <net/message/session/locking/FLockRangeMsgEx.h>
#include <net/message/session/opening/CloseFileMsgEx.h>
#include <net/message/session/opening/OpenFileMsgEx.h>
#include <net/message/storage/attribs/RefreshEntryInfoMsgEx.h>
#include <net/message/storage/attribs/RemoveXAttrMsgEx.h>
#include <net/message/storage/attribs/SetAttrMsgEx.h>
#include <net/message/storage/attribs/SetDirPatternMsgEx.h>
#include <net/message/storage/attribs/SetXAttrMsgEx.h>
#include <net/message/storage/attribs/UpdateDirParentMsgEx.h>
#include <net/message/storage/creating/HardlinkMsgEx.h>
#include <net/message/storage/creating/MkDirMsgEx.h>
#include <net/message/storage/creating/MkFileMsgEx.h>
#include <net/message/storage/creating/MkFileWithPatternMsgEx.h>
#include <net/message/storage/creating/MkLocalDirMsgEx.h>
#include <net/message/storage/creating/RmDirMsgEx.h>
#include <net/message/storage/creating/RmLocalDirMsgEx.h>
#include <net/message/storage/creating/UnlinkFileMsgEx.h>
#include <net/message/storage/lookup/LookupIntentMsgEx.h>
#include <net/message/storage/moving/MovingDirInsertMsgEx.h>
#include <net/message/storage/moving/MovingFileInsertMsgEx.h>
#include <net/message/storage/moving/RenameV2MsgEx.h>
#include <net/message/storage/TruncFileMsgEx.h>

void MirroredMessageResponseState::serialize(Serializer& ser) const
{
   ser % serializerTag();
   serializeContents(ser);
}

std::unique_ptr<MirroredMessageResponseState> MirroredMessageResponseState::deserialize(
   Deserializer& des)
{
   uint32_t tag;

   des % tag;
   if (!des.good())
      return {};

#define HANDLE_TAG(value, type) \
   case value: return boost::make_unique<type::ResponseState>(des);

   switch (tag)
   {
      HANDLE_TAG(NETMSGTYPE_OpenFile,          OpenFileMsgEx)
      HANDLE_TAG(NETMSGTYPE_CloseFile,         CloseFileMsgEx)
      HANDLE_TAG(NETMSGTYPE_TruncFile,         TruncFileMsgEx)
      HANDLE_TAG(NETMSGTYPE_MkFileWithPattern, MkFileWithPatternMsgEx)
      HANDLE_TAG(NETMSGTYPE_Hardlink,          HardlinkMsgEx)
      HANDLE_TAG(NETMSGTYPE_MkLocalDir,        MkLocalDirMsgEx)
      HANDLE_TAG(NETMSGTYPE_UnlinkFile,        UnlinkFileMsgEx)
      HANDLE_TAG(NETMSGTYPE_RmLocalDir,        RmLocalDirMsgEx)
      HANDLE_TAG(NETMSGTYPE_MkDir,             MkDirMsgEx)
      HANDLE_TAG(NETMSGTYPE_MkFile,            MkFileMsgEx)
      HANDLE_TAG(NETMSGTYPE_RmDir,             RmDirMsgEx)
      HANDLE_TAG(NETMSGTYPE_UpdateDirParent,   UpdateDirParentMsgEx)
      HANDLE_TAG(NETMSGTYPE_RemoveXAttr,       RemoveXAttrMsgEx)
      HANDLE_TAG(NETMSGTYPE_SetXAttr,          SetXAttrMsgEx)
      HANDLE_TAG(NETMSGTYPE_SetDirPattern,     SetDirPatternMsgEx)
      HANDLE_TAG(NETMSGTYPE_SetAttr,           SetAttrMsgEx)
      HANDLE_TAG(NETMSGTYPE_RefreshEntryInfo,  RefreshEntryInfoMsgEx)
      HANDLE_TAG(NETMSGTYPE_MovingFileInsert,  MovingFileInsertMsgEx)
      HANDLE_TAG(NETMSGTYPE_MovingDirInsert,   MovingDirInsertMsgEx)
      HANDLE_TAG(NETMSGTYPE_Rename,            RenameV2MsgEx)
      HANDLE_TAG(NETMSGTYPE_LookupIntent,      LookupIntentMsgEx)
      HANDLE_TAG(NETMSGTYPE_AckNotify,         AckNotifiyMsgEx)
      HANDLE_TAG(NETMSGTYPE_FLockEntry,        FLockEntryMsgEx)
      HANDLE_TAG(NETMSGTYPE_FLockRange,        FLockRangeMsgEx)
      HANDLE_TAG(NETMSGTYPE_BumpFileVersion,   BumpFileVersionMsgEx)

      default:
         LOG(MIRRORING, ERR, "bad mirror response state tag.", tag);
         des.setBad();
         break;
   }

   return {};
}

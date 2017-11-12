#include "DisposalCleaner.h"

#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/storage/Metadata.h>
#include <common/toolkit/MessagingTk.h>

void DisposalCleaner::run(NodeStore& nodes, const std::function<OnItemFn>& onItem,
   const std::function<OnErrorFn>& onError)
{
   auto node = nodes.referenceFirstNode();
   while (node)
   {
      FhgfsOpsErr walkRes = walkNode(*node, onItem);
      if (walkRes != FhgfsOpsErr_SUCCESS)
         onError(*node, walkRes);

      node = nodes.referenceNextNode(node);
   }
}

FhgfsOpsErr DisposalCleaner::walkNode(Node& node, const std::function<OnItemFn>& onItem)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   size_t numEntriesThisRound = 0; // received during last RPC round
   bool commRes = true;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   ListDirFromOffsetRespMsg* respMsgCast;

   FhgfsOpsErr listRes = FhgfsOpsErr_SUCCESS;
   unsigned maxOutNames = 50;
   StringList* entryNames;
   uint64_t currentServerOffset = 0;

   do
   {
      for (int i = 0; i <= 1; i++)
      {
         // i == 0 -> non-mirrored metadata
         // i == 1 -> mirrored metadata
         //
         // we have to skip mirrored metadata if the node we are looking at is the secondary if its
         // group, otherwise we would list mirrored disposal file twice - and attempt to delete them
         // twice.
         // also, we don't have to list anything if the node is not part of a mirror group at all.
         if (i == 1)
         {
            const uint16_t thisGroup = bgm->getBuddyGroupID(node.getNumID().val());

            if (thisGroup == 0)
               continue;

            if (bgm->getPrimaryTargetID(thisGroup) != node.getNumID().val())
               continue;
         }

         std::string entryID       = i == 0
            ? META_DISPOSALDIR_ID_STR
            : META_MIRRORDISPOSALDIR_ID_STR;
         EntryInfo entryInfo(node.getNumID(), "", entryID, entryID, DirEntryType_DIRECTORY,
               i == 0 ? 0 : ENTRYINFO_FEATURE_BUDDYMIRRORED);

         ListDirFromOffsetMsg listMsg(&entryInfo, currentServerOffset, maxOutNames, true);

         // request/response
         commRes = MessagingTk::requestResponse(
            node, &listMsg, NETMSGTYPE_ListDirFromOffsetResp, &respBuf, &respMsg);
         if (!commRes)
         {
            retVal = FhgfsOpsErr_COMMUNICATION;
            goto err_cleanup;
         }

         respMsgCast = (ListDirFromOffsetRespMsg*)respMsg;

         listRes = (FhgfsOpsErr)respMsgCast->getResult();
         if (listRes != FhgfsOpsErr_SUCCESS)
         {
            retVal = listRes;
            goto err_cleanup;
         }

         entryNames = &respMsgCast->getNames();

         numEntriesThisRound = entryNames->size();
         currentServerOffset = respMsgCast->getNewServerOffset();

         for (auto it = entryNames->begin(); it != entryNames->end(); ++it)
         {
            retVal = onItem(node, *it, i != 0);
            if (retVal != FhgfsOpsErr_SUCCESS)
               break;
         }

      err_cleanup:
         SAFE_DELETE(respMsg);
         SAFE_FREE(respBuf);
      }
   } while (retVal == FhgfsOpsErr_SUCCESS && numEntriesThisRound == maxOutNames);

   return retVal;
}

FhgfsOpsErr DisposalCleaner::unlinkFile(Node& node, std::string entryName, const bool isMirrored)
{
   FhgfsOpsErr retVal;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   UnlinkFileRespMsg* respMsgCast;

   std::string entryID = isMirrored
      ? META_MIRRORDISPOSALDIR_ID_STR
      : META_DISPOSALDIR_ID_STR;

   EntryInfo parentInfo(node.getNumID(), "", entryID, entryID, DirEntryType_DIRECTORY,
         isMirrored ? ENTRYINFO_FEATURE_BUDDYMIRRORED : 0);

   UnlinkFileMsg getInfoMsg(&parentInfo, entryName);

   // request/response
   commRes = MessagingTk::requestResponse(
      node, &getInfoMsg, NETMSGTYPE_UnlinkFileResp, &respBuf, &respMsg);
   if (!commRes)
   {
      retVal = FhgfsOpsErr_COMMUNICATION;
      goto err_cleanup;
   }

   respMsgCast = (UnlinkFileRespMsg*)respMsg;

   retVal = (FhgfsOpsErr)respMsgCast->getValue();

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

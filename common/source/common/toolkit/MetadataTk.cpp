#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/storage/lookup/FindOwnerMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/storage/Metadata.h>
#include <common/threading/PThread.h>
#include "MetadataTk.h"

static std::shared_ptr<Node> referenceRoot(NodeStoreServers& nodes, const RootInfo& root,
      MirrorBuddyGroupMapper& bgm)
{
   const auto id = root.getID();
   const auto isMirrored = root.getIsMirrored();
   if (!isMirrored)
      return nodes.referenceNode(id);

   return nodes.referenceNode(NumNodeID(bgm.getPrimaryTargetID(id.val())));
}

/**
 * Find and reference the metadata owner node of a certain entry.
 *
 * @param nodes metadata nodes
 * @param outReferencedNode only valid if result is FhgfsOpsErr_SUCCESS (needs to be released
 * through the MetaNodesStore)
 */
FhgfsOpsErr MetadataTk::referenceOwner(Path* searchPath,
   NodeStoreServers* nodes, NodeHandle& outReferencedNode, EntryInfo* outEntryInfo,
   const RootInfo& metaRoot, MirrorBuddyGroupMapper* metaBuddyGroupMapper)
{
   const char* logContext = "MetadataTk::referenceOwner";

   size_t searchDepth = searchPath->size(); // 0-based incl. root

   if(!searchDepth)
   { // looking for the root node (ignore referenceParent)
      outReferencedNode = referenceRoot(*nodes, metaRoot, *metaBuddyGroupMapper);
      if (likely(outReferencedNode))
      { // got root node
         if (metaRoot.getIsMirrored())
            outEntryInfo->set(metaRoot.getID(), "", META_ROOTDIR_ID_STR, "",
               DirEntryType_DIRECTORY, ENTRYINFO_FEATURE_BUDDYMIRRORED);
         else
            outEntryInfo->set(outReferencedNode->getNumID(), "", META_ROOTDIR_ID_STR, "",
               DirEntryType_DIRECTORY, 0);

         return FhgfsOpsErr_SUCCESS;
      }
      else
      { // no root node (yet)
         LogContext(logContext).logErr("Unable to proceed without a working root metadata server");
         return FhgfsOpsErr_UNKNOWNNODE;
      }
   }

   FhgfsOpsErr findRes = findOwner(searchPath, searchDepth, nodes, outEntryInfo, metaRoot,
         metaBuddyGroupMapper);

   if(findRes == FhgfsOpsErr_SUCCESS)
   { // simple success case => try to reference the node
     // in case of mirrored entry, the primary node will be referenced
      NumNodeID referenceNodeID;
      if ( outEntryInfo->getIsBuddyMirrored() )
         referenceNodeID = NumNodeID(
            metaBuddyGroupMapper->getPrimaryTargetID(outEntryInfo->getOwnerNodeID().val() ) );
      else
         referenceNodeID = outEntryInfo->getOwnerNodeID();

      outReferencedNode = nodes->referenceNode(referenceNodeID);
      if (unlikely(!outReferencedNode))
      {
         LogContext(logContext).logErr("Unknown metadata node: " + referenceNodeID.str());

         findRes = FhgfsOpsErr_UNKNOWNNODE;
      }
   }

   return findRes;
}

/**
 * Peforms a single node request as part of the incremental metadata owner node search.
 *
 * @param outInfo is only valid if return is FhgfsOpsErr_SUCCESS (outInfo strings are kalloced
 * and need to be kfreed by the caller)
 */
inline FhgfsOpsErr MetadataTk::findOwnerStep(Node& node,
   NetMessage* requestMsg, EntryInfoWithDepth* outEntryInfoWDepth)
{
   FindOwnerRespMsg* findResp;
   FhgfsOpsErr findRes;

   const auto respMsg = MessagingTk::requestResponse(node, *requestMsg, NETMSGTYPE_FindOwnerResp);
   if (!respMsg)
   { // communication error
      return FhgfsOpsErr_INTERNAL;
   }

   // handle result
   findResp = (FindOwnerRespMsg*)respMsg.get();
   findRes = (FhgfsOpsErr)findResp->getResult();

   if(findRes == FhgfsOpsErr_SUCCESS)
      *outEntryInfoWDepth = findResp->getEntryInfo();

   return findRes;
}

/**
 * Incremental search for a metadata owner node.
 *
 * @param nodes metadata nodes
 * @param outOwner is only valid if return is FhgfsOpsErr_SUCCESS (and needs to be kfreed by the
 * caller)
 */
FhgfsOpsErr MetadataTk::findOwner(Path* searchPath, unsigned searchDepth, NodeStoreServers* nodes,
   EntryInfo* outEntryInfo, const RootInfo& metaRoot, MirrorBuddyGroupMapper* metaBuddyGroupMapper)
{
   const char* logContextStr = "MetadataTk::findOwner";

   // reference root node
   auto currentNode = referenceRoot(*nodes, metaRoot, *metaBuddyGroupMapper);
   if (unlikely(!currentNode))
   {
      LogContext(logContextStr).logErr("Unable to proceed without a working root metadata server");
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   EntryInfo lastEntryInfo(currentNode->getNumID(), "", META_ROOTDIR_ID_STR,
      searchPath->back(), DirEntryType_DIRECTORY, 0);

   unsigned lastEntryDepth = 0;
   unsigned numSearchSteps = 0;
   FhgfsOpsErr findRes;

   // walk over all the path nodes (taking shortcuts when possible)
   // (note: do not exit the loop via the for-line, because a node is referenced at that point
   // and it is unclear whether the outInfo strings need to be kfreed)
   for( ; ; numSearchSteps++)
   {
      LOG_DEBUG(logContextStr, Log_SPAM, "path: " + searchPath->str() + "; "
         "searchDepth: " + StringTk::uintToStr(searchDepth) + "; "
         "lastEntryID: " + lastEntryInfo.getEntryID() + "; "
         "currentDepth: " + StringTk::uintToStr(lastEntryDepth) + "; ");

      FindOwnerMsg requestMsg(searchPath, searchDepth, &lastEntryInfo, lastEntryDepth);

      EntryInfoWithDepth entryInfoWDepth;
      findRes = findOwnerStep(*currentNode, &requestMsg, &entryInfoWDepth);

      if(findRes != FhgfsOpsErr_SUCCESS)
      { // unsuccessful end of search
         goto global_cleanup;
      }

      if(entryInfoWDepth.getEntryDepth() == searchDepth)
      { // successful end of search
         outEntryInfo->set(&entryInfoWDepth);
         goto global_cleanup;
      }

      if(unlikely(entryInfoWDepth.getEntryDepth() <= lastEntryDepth) )
      { // something went wrong (probably because of concurrent access)
         findRes = FhgfsOpsErr_INTERNAL;
         goto entryinfo_cleanup;
      }

      if(unlikely(numSearchSteps > METADATATK_OWNERSEARCH_MAX_STEPS) )
      { // search is taking too long - are we trapped in a circle? => better cancel
         LogContext(logContextStr).logErr("Max search steps exceeded for path: " +
            searchPath->str());
         findRes = FhgfsOpsErr_INTERNAL;
         goto entryinfo_cleanup;
      }

      // search not finished yet => prepare for next round (proceed with next node)
      if ( entryInfoWDepth.getIsBuddyMirrored() )
         currentNode = nodes->referenceNode(NumNodeID(
            metaBuddyGroupMapper->getPrimaryTargetID(entryInfoWDepth.getOwnerNodeID().val() ) ) );
      else
         currentNode = nodes->referenceNode(entryInfoWDepth.getOwnerNodeID());

      if(!currentNode)
      { // search cannot continue because the next node is unknown
         LogContext(logContextStr).logErr("Unable to retrieve metadata because of unknown node: " +
            entryInfoWDepth.getOwnerNodeID().str() );
         findRes = FhgfsOpsErr_UNKNOWNNODE;
         goto entryinfo_cleanup;
      }

      lastEntryInfo = entryInfoWDepth; // NOLINT(cppcoreguidelines-slicing)
      lastEntryDepth = entryInfoWDepth.getEntryDepth();
   }

   // clean-up
entryinfo_cleanup:
   // nothing to be done here in the c++ version

global_cleanup:
   // nothing to be done here in the c++ version

   return findRes;
}

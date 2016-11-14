#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/net/message/storage/lookup/FindOwnerMsg.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include <common/storage/Metadata.h>
#include <common/threading/PThread.h>
#include "MetadataTk.h"

/**
 * Find and reference the metadata owner node of a certain entry.
 *
 * @param referenceParent true to reference the parent of the owner instead of the actual owner
 * @param nodes metadata nodes
 * @param outReferencedNode only valid if result is FhgfsOpsErr_SUCCESS (needs to be released
 * through the MetaNodesStore)
 */
FhgfsOpsErr MetadataTk::referenceOwner(Path* searchPath, bool referenceParent,
   NodeStoreServers* nodes, Node** outReferencedNode, EntryInfo* outEntryInfo)
{
   const char* logContext = "MetadataTk::referenceOwner";

   size_t searchDepth = searchPath->getNumPathElems(); // 0-based incl. root
   
   if(!searchDepth)
   { // looking for the root node (ignore referenceParent)

      *outReferencedNode = nodes->referenceRootNode();
      if(likely(*outReferencedNode) )
      { // got root node
         outEntryInfo->update( (*outReferencedNode)->getNumID(), "", META_ROOTDIR_ID_STR, "",
            DirEntryType_DIRECTORY, 0);

         return FhgfsOpsErr_SUCCESS;
      }
      else
      { // no root node (yet)
         LogContext(logContext).logErr("Unable to proceed without a working root metadata server");
         return FhgfsOpsErr_UNKNOWNNODE;
      }
   }

   FhgfsOpsErr findRes = findOwner(searchPath,
      referenceParent ? (searchDepth - 1) : searchDepth, nodes, outEntryInfo);

   if(findRes == FhgfsOpsErr_SUCCESS)
   { // simple success case => try to reference the node
      *outReferencedNode = nodes->referenceNode(outEntryInfo->getOwnerNodeID() );
      if(unlikely(!*outReferencedNode) )
      {
         LogContext(logContext).logErr("Unknown metadata node: " +
            StringTk::uintToStr(outEntryInfo->getOwnerNodeID() ) );

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
inline FhgfsOpsErr MetadataTk::findOwnerStep(Path* searchPath, Node* node,
   NetMessage* requestMsg, EntryOwnerInfo* outEntryOwnerInfo)
{
   char* respBuf;
   NetMessage* respMsg;
   bool requestRes;
   FindOwnerRespMsg* findResp;
   FhgfsOpsErr findRes;

   // connect & communicate
   requestRes = MessagingTk::requestResponse(
      node, requestMsg, NETMSGTYPE_FindOwnerResp, &respBuf, &respMsg);

   if(unlikely(!requestRes) )
   { // communication error
      return FhgfsOpsErr_INTERNAL;
   }
   
   // handle result
   findResp = (FindOwnerRespMsg*)respMsg;
   findRes = (FhgfsOpsErr)findResp->getResult();
   
   if(findRes == FhgfsOpsErr_SUCCESS)
      findResp->getOwnerInfo(outEntryOwnerInfo);
   //else
      //Logger_logFormatted(log, 3, logContext, "FindOwnerResp error code: %d", findRes);
      
   // clean-up
   delete(respMsg);
   free(respBuf);
   
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
   EntryInfo* outEntryInfo)
{
   const char* logContextStr = "MetadataTk::findOwner";

   // reference root node
   Node* currentNode = nodes->referenceRootNode();
   if(unlikely(!currentNode) )
   {
      LogContext(logContextStr).logErr("Unable to proceed without a working root metadata server");
      return FhgfsOpsErr_UNKNOWNNODE;
   }
   
   EntryInfo lastEntryInfo(currentNode->getNumID(), "", META_ROOTDIR_ID_STR,
      searchPath->getLastElem(), DirEntryType_DIRECTORY, 0);

   unsigned lastEntryDepth = 0;
   unsigned numSearchSteps = 0;
   FhgfsOpsErr findRes;

   // walk over all the path nodes (taking shortcuts when possible)
   // (note: do not exit the loop via the for-line, because a node is referenced at that point
   // and it is unclear whether the outInfo strings need to be kfreed)
   for( ; ; numSearchSteps++)
   {
      LOG_DEBUG(logContextStr, Log_SPAM, "path: " + searchPath->getPathAsStr() + "; "
         "searchDepth: " + StringTk::uintToStr(searchDepth) + "; "
         "lastEntryID: " + lastEntryInfo.getEntryID() + "; "
         "currentDepth: " + StringTk::uintToStr(lastEntryDepth) + "; ");
      
      FindOwnerMsg requestMsg(searchPath, searchDepth, &lastEntryInfo, lastEntryDepth);

      EntryOwnerInfo entryOwnerInfo;
      findRes = findOwnerStep(searchPath, currentNode, &requestMsg, &entryOwnerInfo);
      
      nodes->releaseNode(&currentNode);
      
      if(findRes != FhgfsOpsErr_SUCCESS)
      { // unsuccessful end of search
         goto global_cleanup;
      }
      
      if(entryOwnerInfo.getEntryDepth() == searchDepth)
      { // successful end of search
         outEntryInfo->updateFromOwnerInfo(&entryOwnerInfo);
         goto global_cleanup;
      }
      
      if(unlikely(entryOwnerInfo.getEntryDepth() <= lastEntryDepth) )
      { // something went wrong (probably because of concurrent access)
         findRes = FhgfsOpsErr_INTERNAL;
         goto entryinfo_cleanup;
      }
      
      if(unlikely(numSearchSteps > METADATATK_OWNERSEARCH_MAX_STEPS) )
      { // search is taking too long - are we trapped in a circle? => better cancel
         LogContext(logContextStr).logErr("Max search steps exceeded for path: " +
            searchPath->getPathAsStr() );
         findRes = FhgfsOpsErr_INTERNAL;
         goto entryinfo_cleanup;
      }
      
      // search not finished yet => prepare for next round (proceed with next node)
      
      currentNode = nodes->referenceNode(entryOwnerInfo.getOwnerNodeID() );
      if(!currentNode)
      { // search cannot continue because the next node is unknown
         LogContext(logContextStr).logErr("Unable to retrieve metadata because of unknown node: " +
            StringTk::uintToStr(entryOwnerInfo.getOwnerNodeID() ) );
         findRes = FhgfsOpsErr_UNKNOWNNODE;
         goto entryinfo_cleanup;
      }
      
      lastEntryInfo.updateFromOwnerInfo(&entryOwnerInfo);
      lastEntryDepth = entryOwnerInfo.getEntryDepth();
   }

   // clean-up
entryinfo_cleanup:
   // nothing to be done here in the c++ version

global_cleanup:
   // nothing to be done here in the c++ version
   
   return findRes;
}

#include "FindLinkOwnerMsgEx.h"

#include <program/Program.h>

bool FindLinkOwnerMsgEx::processIncoming(ResponseContext& ctx)
{
   MetaStore *metaStore = Program::getApp()->getMetaStore();
   std::string entryID = this->getEntryID();

   int result = 1;
   std::string parentEntryID;
   NumNodeID parentNodeID;

   MetaFileHandle file;
   DirInode *dir = NULL;
   // we don't know if the ID belongs to a file or a directory, so we try the file first and if that
   // does not work, we try directory

   /* TODO: Used by fhgfs-ctl ModeReverseLookup, but this mode does not support to send the
    *       parentID. With the 2014.01 format this should be possible for most files/chunks, though.
    */

   // TODO: buddy mirroring => but this is not used anymore, so maybe it's better to just delete it
   file = metaStore->referenceLoadedFile(parentEntryID, false, entryID);
   if (file != NULL)
   {
      // file->getParentInfo(&parentEntryID, &parentNodeID);
      result = 0;
      metaStore->releaseFile(parentEntryID, file);
      goto send_response;
   }
   // TODO: buddy mirroring => but this is not used anymore, so maybe it's better to just delete it
   dir = metaStore->referenceDir(entryID, false, true);
   if (dir != NULL)
   {
      dir->getParentInfo(&parentEntryID, &parentNodeID);
      result = 0;
      metaStore->releaseDir(entryID);
      goto send_response;
   }

send_response:
   ctx.sendResponse(FindLinkOwnerRespMsg(result, parentNodeID, parentEntryID) );

   return true;
}


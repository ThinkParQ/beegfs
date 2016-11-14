#include "FindEntrynameMsgEx.h"

#include <program/Program.h>

// TODO Christian: get this working again!


bool FindEntrynameMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "FindEntrynameMsg incoming";

   LOG_DEBUG(logContext, 4, "Received a FindEntrynameMsg from: " + ctx.peerName() );

   LogContext(logContext).log(Log_WARNING, "Message handler not implemented");

   return false;

   /*
   MetaStore *metaStore = Program::getApp()->getMetaStore();
   std::string id = this->getID();
   std::string forcedDirID = this->getDirID();
   std::string outName = "";
   std::string outParentID = "";

   bool stop = false;
   long currentDirLoc = getCurrentDirLoc();
   long lastEntryLoc = getLastEntryLoc();
   bool moreToCome = false;

   time_t secondsStart = time(NULL);

   if (forcedDirID.compare("") != 0)
   {
      // check only this dirID!
      std::string dirID = forcedDirID;
      // look into the .cont directory
      uint resultSize = 0;
      DirInode *dir = metaStore->referenceDir(dirID, true);
      if (dir != NULL)
      {
         int64_t offset = 0;

         // get the contents of the directory
         do
         {
            StringList dirContents;
            int64_t outOffset = 0;
            dir->listIncremental(offset, 0, 1000, &dirContents, &outOffset);
            offset = outOffset;
            resultSize = dirContents.size();

            // for every entry that is read, compare it to the input id
            for (StringListIter nameIter = dirContents.begin(); nameIter != dirContents.end(); nameIter++)
            {
               DirEntry dirEntry;
               std::string nameStr = *nameIter;
               dir->getDentry(nameStr, dirEntry);
               // StringListIter idIter = inIdList.begin();

               if (dirEntry.getID().compare(id) == 0)
               {
                  stop = true;
                  moreToCome = false;
                  outName = nameStr;
                  outParentID = dirID;
                  metaStore->releaseDir(dir);
                  goto send_response;
               }
            }
         } while (resultSize > 0);

         metaStore->releaseDir(dir);
      }

   }
   else
   {
      //no specific dir ID => run over all directories
      moreToCome = true;


      while (!stop)
      {
         long outCurrentDirLoc = 0;
         long outLastEntryLoc = 0;
         // get the next .cont directory
         std::string dirID = "";
         if (StorageTkEx::getNextContDirID(currentDirLoc, lastEntryLoc, &outCurrentDirLoc,
            &outLastEntryLoc, &dirID))
         {
            currentDirLoc = outCurrentDirLoc;
            lastEntryLoc = outLastEntryLoc;

            // look into the .cont directory
            uint resultSize = 0;
            DirInode *dir = metaStore->referenceDir(dirID, true);
            if (dir == NULL)
               continue;
            int64_t offset = 0;

            // get the contents of the directory
            do
            {
               StringList dirContents;
               int64_t outOffset = 0;
               dir->listIncremental(offset, 0, 100, &dirContents, &outOffset);
               offset = outOffset;
               resultSize = dirContents.size();

               // for every entry that is read, compare it to the input id
               for (StringListIter nameIter = dirContents.begin(); nameIter != dirContents.end(); nameIter++)
               {
                  DirEntry dirEntry;
                  std::string nameStr = *nameIter;
                  dir->getDentry(nameStr, dirEntry);
                  // StringListIter idIter = inIdList.begin();

                  if (dirEntry.getID().compare(id) == 0)
                  {
                     stop = true;
                     moreToCome = false;
                     outName = nameStr;
                     outParentID = dirID;
                     metaStore->releaseDir(dir);
                     goto send_response;
                  }
               }
            } while (resultSize > 0);

            metaStore->releaseDir(dir);

            // if the maximum amount of time that one message is allowed to take is reached, stop here
            // and send a response
            time_t secondsNow = time(NULL);

            if ((secondsNow - secondsStart) > MAX_SECONDS_PER_MSG)
            {
               stop = true;
            }
         }
         else
         {
            stop = true;
            moreToCome = false;
         }
      }
   }

   send_response: FindEntrynameRespMsg respMsg(moreToCome, outName, outParentID, currentDirLoc,
      lastEntryLoc);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));
*/
   return true;
}


#ifndef STATMSGEX_H_
#define STATMSGEX_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/StatMsg.h>

// The "getattr" operation for linux-kernel files ystems

class StatMsgEx : public StatMsg
{
   public:
      StatMsgEx() : StatMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr statRec(Path& path, StatData& outStatData);
      FhgfsOpsErr statRoot(StatData& outStatData);
      void refreshFileSize(std::string entryID);
      void refreshFileSizeSequential(FileInode* file, std::string entryID);
      void refreshFileSizeParallel(FileInode* file, std::string entryID);

};

#endif /*STATMSGEX_H_*/

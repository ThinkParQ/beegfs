#ifndef GENERICDEBUGMSGEX_H_
#define GENERICDEBUGMSGEX_H_

#include <common/net/message/nodes/GenericDebugMsg.h>


class GenericDebugMsgEx : public GenericDebugMsg
{
   public:
      GenericDebugMsgEx() : GenericDebugMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   private:
      std::string processCommand();

      std::string processOpListFileEntryLocks(std::istringstream& commandStream);
      std::string processOpListFileRangeLocks(std::istringstream& commandStream);
      std::string processOpListOpenFiles(std::istringstream& commandStream);
      std::string processOpReferenceStatistics(std::istringstream& commandStream);
      std::string processOpCacheStatistics(std::istringstream& commandStream);
      std::string processOpVersion(std::istringstream& commandStream);
      std::string processOpDumpDentry(std::istringstream& commandStream);
      std::string processOpWriteDirDentry(std::istringstream& commandStream);
      std::string processOpDumpInode(std::istringstream& commandStream);
      std::string processOpDumpInlinedInode(std::istringstream& commandStream);
      std::string processOpWriteDirInode(std::istringstream& commandStream);
      std::string processOpWriteInlinedFileInode(std::istringstream& commandStream);
};

#endif /* GENERICDEBUGMSGEX_H_ */

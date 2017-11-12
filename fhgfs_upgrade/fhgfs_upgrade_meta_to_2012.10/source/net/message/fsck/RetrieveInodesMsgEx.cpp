#include "RetrieveInodesMsgEx.h"

#include <program/Program.h>

bool RetrieveInodesMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("Incoming RetrieveDirEntriesMsg");
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RetrieveDirEntriesMsg from: ") + peer);

   MetaStore *metaStore = Program::getApp()->getMetaStore();

   unsigned hashDirNum = getHashDirNum();
   unsigned maxOutInodes = getMaxOutInodes();
   int64_t lastOffset = getLastOffset();
   int64_t newOffset;

   FsckFileInodeList fileInodesOutgoing;
   FsckDirInodeList dirInodesOutgoing;

   metaStore->getAllInodesIncremental(hashDirNum, lastOffset, maxOutInodes, &dirInodesOutgoing,
      &fileInodesOutgoing, &newOffset);

   RetrieveInodesRespMsg respMsg(&fileInodesOutgoing, &dirInodesOutgoing, newOffset);

   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}

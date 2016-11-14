#ifndef FINDLINKOWNERMSGEX_H
#define FINDLINKOWNERMSGEX_H

#include <common/net/message/storage/lookup/FindLinkOwnerMsg.h>
#include <common/net/message/storage/lookup/FindLinkOwnerRespMsg.h>

class FindLinkOwnerMsgEx : public FindLinkOwnerMsg
{
   public:
      FindLinkOwnerMsgEx() : FindLinkOwnerMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats) throw (SocketException);

   protected:
      
   private:
};

#endif /*FINDLINKOWNERMSGEX_H*/

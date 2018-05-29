#ifndef FETCHFSCKCHUNKLISTMSG_H
#define FETCHFSCKCHUNKLISTMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/serialization/Serialization.h>

class FetchFsckChunkListMsg: public NetMessageSerdes<FetchFsckChunkListMsg>
{
   public:
      /*
       * @param maxNumChunks max number of chunks to fetch at once
       */
      FetchFsckChunkListMsg(unsigned maxNumChunks, FetchFsckChunkListStatus lastStatus,
            bool forceRestart) :
         BaseType(NETMSGTYPE_FetchFsckChunkList),
         maxNumChunks(maxNumChunks),
         lastStatus(lastStatus),
         forceRestart(forceRestart)
      { }

      // only for deserialization
      FetchFsckChunkListMsg() : BaseType(NETMSGTYPE_FetchFsckChunkList)
      { }

   private:
      uint32_t maxNumChunks;
      FetchFsckChunkListStatus lastStatus;
      bool forceRestart;

   public:
      unsigned getMaxNumChunks() const { return maxNumChunks; }
      FetchFsckChunkListStatus getLastStatus() const { return lastStatus; }
      bool getForceRestart() const { return forceRestart; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->maxNumChunks
            % serdes::as<uint32_t>(obj->lastStatus)
            % obj->forceRestart;
      }
};

#endif /*FETCHFSCKCHUNKLISTMSG_H*/

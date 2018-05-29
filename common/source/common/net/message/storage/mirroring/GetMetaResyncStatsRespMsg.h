#ifndef COMMON_GETMETARESYNCSTATSRESPMSG_H
#define COMMON_GETMETARESYNCSTATSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/mirroring/BuddyResyncJobStatistics.h>

class GetMetaResyncStatsRespMsg : public NetMessageSerdes<GetMetaResyncStatsRespMsg>
{
   public:
      GetMetaResyncStatsRespMsg(MetaBuddyResyncJobStatistics* jobStats) :
         BaseType(NETMSGTYPE_GetMetaResyncStatsResp),
         jobStatsPtr(jobStats)
      {}

      GetMetaResyncStatsRespMsg() : BaseType(NETMSGTYPE_FsckSetEventLoggingResp) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->jobStatsPtr, obj->jobStats);
      }

   private:
      MetaBuddyResyncJobStatistics* jobStatsPtr;
      MetaBuddyResyncJobStatistics jobStats;

   public:
      MetaBuddyResyncJobStatistics* getJobStats()
      {
         return &jobStats;
      }

      void getJobStats(MetaBuddyResyncJobStatistics& outJobStats)
      {
         outJobStats = jobStats;
      }
};

#endif /* COMMON_GETMETARESYNCSTATSRESPMSG_H */

#ifndef GETSTORAGERESYNCSTATSRESPMSG_H_
#define GETSTORAGERESYNCSTATSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/mirroring/BuddyResyncJobStatistics.h>

class GetStorageResyncStatsRespMsg : public NetMessageSerdes<GetStorageResyncStatsRespMsg>
{
   public:
      /*
       * @param jobStats not owned by this object!
       */
      GetStorageResyncStatsRespMsg(StorageBuddyResyncJobStatistics* jobStats) :
         BaseType(NETMSGTYPE_GetStorageResyncStatsResp)
      {
         this->jobStatsPtr = jobStats;
      }

      /**
       * For deserialization only!
       */
      GetStorageResyncStatsRespMsg() : BaseType(NETMSGTYPE_GetStorageResyncStatsResp)
      {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->jobStatsPtr, obj->jobStats);
      }

   private:
      // for serialization
      StorageBuddyResyncJobStatistics* jobStatsPtr;

      // for deserialization
      StorageBuddyResyncJobStatistics jobStats;

   public:
      // getters & setters
      StorageBuddyResyncJobStatistics* getJobStats()
      {
         return &jobStats;
      }

      void getJobStats(StorageBuddyResyncJobStatistics& outStats)
      {
         outStats = jobStats;
      }
};

#endif /*GETSTORAGERESYNCSTATSRESPMSG_H_*/

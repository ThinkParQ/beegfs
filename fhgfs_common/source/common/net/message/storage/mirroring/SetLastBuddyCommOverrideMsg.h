#ifndef SETLASTBUDDYCOMMOVERRIDEMSG_H_
#define SETLASTBUDDYCOMMOVERRIDEMSG_H_

#include <common/net/message/NetMessage.h>

class SetLastBuddyCommOverrideMsg : public NetMessageSerdes<SetLastBuddyCommOverrideMsg>
{
   public:

      /**
       * @param targetID
       * @param timestamp
       * @param abortResync if a resync is already running for that target abort it, so that it
       * restarts with the new timestamp file
       */
      SetLastBuddyCommOverrideMsg(uint16_t targetID, int64_t timestamp, bool abortResync) :
         BaseType(NETMSGTYPE_SetLastBuddyCommOverride), targetID(targetID), timestamp(timestamp),
         abortResync(abortResync)
      {
      }

      /**
       * For deserialization only!
       */
      SetLastBuddyCommOverrideMsg() : BaseType(NETMSGTYPE_SetLastBuddyCommOverride) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->targetID
            % obj->timestamp
            % obj->abortResync;
      }

   private:
      uint16_t targetID;
      int64_t timestamp;
      bool abortResync;

   public:
      // getters & setters
      uint16_t getTargetID() const
      {
         return targetID;
      }

      int64_t getTimestamp() const
      {
         return timestamp;
      }

      bool getAbortResync() const
      {
         return abortResync;
      }

};

#endif /*SETLASTBUDDYCOMMOVERRIDEMSG_H_*/

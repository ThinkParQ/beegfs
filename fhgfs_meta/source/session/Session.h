#ifndef SESSION_H_
#define SESSION_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <session/MirrorMessageResponseState.h>
#include "SessionFileStore.h"

#include <mutex>

struct MirrorStateSlot
{
   std::unique_ptr<MirroredMessageResponseState> response;

   friend Serializer& operator%(Serializer& ser, const std::shared_ptr<MirrorStateSlot>& obj)
   {
      ser % bool(obj->response);
      if (obj->response)
         obj->response->serialize(ser);

      return ser;
   }

   friend Deserializer& operator%(Deserializer& des, std::shared_ptr<MirrorStateSlot>& obj)
   {
      bool hasContent;

      obj.reset(new MirrorStateSlot);

      des % hasContent;
      if (des.good() && hasContent)
         obj->response = MirroredMessageResponseState::deserialize(des);

      return des;
   }
};

/*
 * A session always belongs to a client ID, therefore the session ID is always the nodeID of the
 * corresponding client
 */
class Session
{
   public:
      Session(NumNodeID sessionID) : sessionID(sessionID) {}

      /*
       * For deserialization only
       */
      Session() {};

      void mergeSessionFiles(Session* session);

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->sessionID
            % obj->files
            % obj->mirrorProcessState;

         obj->dropEmptyStateSlots(ctx);
      }

      bool operator==(const Session& other) const;

      bool operator!=(const Session& other) const { return !(*this == other); }

   private:
      NumNodeID sessionID;

      SessionFileStore files;

      Mutex mirrorProcessStateLock;
      // mirror state slots are shared_ptrs for two reasons: ordering of memory operations, and
      // misbehaving clients.
      //  * memory ordering turns into a very easy problem if every user of an object holds a
      //    reference that will keep the object alive, even if it is removed from this map for
      //    some reason.
      //  * clients may reuse sequences numbers in violation of the protocol, or clear out sequence
      //    numbers that are still processing on the server. in the first case, we will want an
      //    ephemeral response state that will immediatly go away, in the second we want to keep
      //    the response state alive as long as some thread is still operating on it.
      std::map<uint64_t, std::shared_ptr<MirrorStateSlot>> mirrorProcessState;

      void dropEmptyStateSlots(Serializer&) const
      {
      }

      // when the session state for any given client is loaded, the server must discard empty state
      // slots. if empty slots are not discarded, requests for any such sequence number would be
      // answered with "try again later" - forever, because the slot will never be filled.
      // this is not an issue for resync, because during session resync, all slots are filled.
      // empty slots can only occur if a server was shut down while messages were still being
      // processed.
      void dropEmptyStateSlots(Deserializer&)
      {
         auto it = mirrorProcessState.begin();
         auto end = mirrorProcessState.end();

         while (it != end)
         {
            if (it->second->response)
            {
               ++it;
               continue;
            }

            auto slot = it;

            ++it;
            mirrorProcessState.erase(slot);
         }
      }

   public:
      // getters & setters
      NumNodeID getSessionID()
      {
         return sessionID;
      }

      SessionFileStore* getFiles()
      {
         return &files;
      }

      bool relinkInodes(MetaStore& store)
      {
         return files.relinkInodes(store);
      }

      void freeMirrorStateSlot(uint64_t seqNo)
      {
         std::lock_guard<Mutex> lock(mirrorProcessStateLock);

         mirrorProcessState.erase(seqNo);
      }

      std::pair<std::shared_ptr<MirrorStateSlot>, bool> acquireMirrorStateSlot(uint64_t endSeqno,
         uint64_t thisSeqno)
      {
         std::lock_guard<Mutex> lock(mirrorProcessStateLock);

         // the client sets endSeqno to the largest sequence number it knows to be fully processed,
         // with no sequence numbers below it still being active somewhere. we can remove all states
         // with smaller sequence numbers and endSeqno itself
         mirrorProcessState.erase(
               mirrorProcessState.begin(),
               mirrorProcessState.lower_bound(endSeqno + 1));

         auto inserted = mirrorProcessState.insert(
               {thisSeqno, std::make_shared<MirrorStateSlot>()});
         return {inserted.first->second, inserted.second};
      }

      std::pair<std::shared_ptr<MirrorStateSlot>, bool> acquireMirrorStateSlotSelective(
         uint64_t finishedSeqno, uint64_t thisSeqno)
      {
         std::lock_guard<Mutex> lock(mirrorProcessStateLock);

         mirrorProcessState.erase(finishedSeqno);

         auto inserted = mirrorProcessState.insert(
               {thisSeqno, std::make_shared<MirrorStateSlot>()});
         return {inserted.first->second, inserted.second};
      }

      uint64_t getSeqNoBase()
      {
         std::lock_guard<Mutex> lock(mirrorProcessStateLock);

         if (!mirrorProcessState.empty())
            return mirrorProcessState.rbegin()->first + 1;
         else
            return 1;
      }
};


#endif /*SESSION_H_*/

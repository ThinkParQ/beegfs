#ifndef FINDOWNERMSG_H_
#define FINDOWNERMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/Path.h>

class FindOwnerMsg : public NetMessageSerdes<FindOwnerMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      FindOwnerMsg(Path* path, unsigned searchDepth, EntryInfo* entryInfo, int currentDepth)
      : BaseType(NETMSGTYPE_FindOwner)
      {
         this->path = path;
         this->searchDepth = searchDepth;

         this->entryInfoPtr = entryInfo;

         this->currentDepth = currentDepth;
      }

      FindOwnerMsg() : BaseType(NETMSGTYPE_FindOwner)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->searchDepth
            % obj->currentDepth
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->path, obj->parsed.path);
      }

   private:
      uint32_t searchDepth;
      uint32_t currentDepth;

      // for serialization
      Path* path; // not owned by this object!
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      struct {
         Path path;
      } parsed;
      EntryInfo entryInfo;


   public:
      Path& getPath()
      {
         return *path;
      }

      unsigned getSearchDepth()
      {
         return searchDepth;
      }

      unsigned getCurrentDepth()
      {
         return currentDepth;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

};


#endif /*FINDOWNERMSG_H_*/

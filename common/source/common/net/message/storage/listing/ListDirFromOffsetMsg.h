#ifndef LISTDIRFROMOFFSET_H_
#define LISTDIRFROMOFFSET_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>


/**
 * Incremental directory listing, returning a limited number of entries each time.
 */
class ListDirFromOffsetMsg : public NetMessageSerdes<ListDirFromOffsetMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       * @param serverOffset zero-based, in incremental calls use only values returned via
       * ListDirFromOffsetResp here (because offset is not guaranteed to be 0, 1, 2, 3, ...).
       * @param filterDots true if you don't want "." and ".." as names in the result list.
       */
      ListDirFromOffsetMsg(EntryInfo* entryInfo, int64_t serverOffset,
         unsigned maxOutNames, bool filterDots) : BaseType(NETMSGTYPE_ListDirFromOffset)
      {
         this->entryInfoPtr = entryInfo;

         this->serverOffset = serverOffset;

         this->maxOutNames = maxOutNames;

         this->filterDots = filterDots;
      }

      /**
       * For deserialization only
       */
      ListDirFromOffsetMsg() : BaseType(NETMSGTYPE_ListDirFromOffset)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->serverOffset
            % obj->maxOutNames
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->filterDots;
      }

   private:
      int64_t serverOffset;
      uint32_t maxOutNames;
      bool filterDots;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;


   public:
      // getters & setters

      int64_t getServerOffset() const
      {
         return serverOffset;
      }

      unsigned getMaxOutNames() const
      {
         return maxOutNames;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      bool getFilterDots() const
      {
         return filterDots;
      }


};


#endif /*LISTDIRFROMOFFSET_H_*/

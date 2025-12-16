#ifndef LISTDIRFROMOFFSET_H_
#define LISTDIRFROMOFFSET_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

// Request compat flag: client supports buffer size mode
// Using compat flags for backward compatibility (old servers ignore unknown compat flags)
#define LISTDIROFFSETMSG_COMPATFLAG_CLIENT_SUPPORTS_BUFSIZE  1

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
       * @param dirListLimit buffer size (bytes) in buffer-size mode, entry count in legacy mode
       * @param filterDots true if you don't want "." and ".." as names in the result list.
       */
      ListDirFromOffsetMsg(EntryInfo* entryInfo, int64_t serverOffset,
         unsigned dirListLimit, bool filterDots) : BaseType(NETMSGTYPE_ListDirFromOffset)
      {
         this->entryInfoPtr = entryInfo;

         this->serverOffset = serverOffset;

         this->dirListLimit = dirListLimit;

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
            % obj->dirListLimit
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->filterDots;
      }

   private:
      int64_t serverOffset;
      uint32_t dirListLimit;
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

      unsigned getDirListLimit() const
      {
         return dirListLimit;
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

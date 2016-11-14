#ifndef LISTDIRFROMOFFSET_H_
#define LISTDIRFROMOFFSET_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class ListDirFromOffsetMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param serverOffset zero-based, in incremental calls use only values returned via
       * ListDirMsgOffsetResp here (because offset is not guaranteed to be 0, 1, 2, 3, ...).
       * @param incrementalOffset zero-based; only used if serverOffset is -1; should be avoided
       */
      ListDirFromOffsetMsg(EntryInfo* entryInfo, int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames) : NetMessage(NETMSGTYPE_ListDirFromOffset)
      {
         this->entryInfoPtr = entryInfo;
         
         this->serverOffset = serverOffset;
         this->incrementalOffset = incrementalOffset;

         this->maxOutNames = maxOutNames;
      }


   protected:
      /**
       * For deserialization only
       */
      ListDirFromOffsetMsg() : NetMessage(NETMSGTYPE_ListDirFromOffset)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            this->entryInfoPtr->serialLen() +
            Serialization::serialLenInt64() + // serverOffset
            Serialization::serialLenUInt64() + // incrementalOffset
            Serialization::serialLenUInt(); // maxOutNames
      }


   private:
      int64_t serverOffset;
      uint64_t incrementalOffset;
      unsigned maxOutNames;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;


   public:
   
      // inliners   
      
      // getters & setters

      int64_t getServerOffset()
      {
         return serverOffset;
      }
      
      uint64_t getIncrementalOffset()
      {
         return incrementalOffset;
      }

      unsigned getMaxOutNames()
      {
         return maxOutNames;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }


};


#endif /*LISTDIRFROMOFFSET_H_*/

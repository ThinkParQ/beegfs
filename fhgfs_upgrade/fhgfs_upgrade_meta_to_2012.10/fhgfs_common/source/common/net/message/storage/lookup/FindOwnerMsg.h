#ifndef FINDOWNERMSG_H_
#define FINDOWNERMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class FindOwnerMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      FindOwnerMsg(Path* path, unsigned searchDepth, EntryInfo* entryInfo, int currentDepth)
      : NetMessage(NETMSGTYPE_FindOwner)
      {
         this->path = path;
         this->searchDepth = searchDepth;

         this->entryInfoPtr = entryInfo;

         this->currentDepth = currentDepth;
      }

   protected:
      FindOwnerMsg() : NetMessage(NETMSGTYPE_FindOwner)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt() + // searchDepth
            Serialization::serialLenUInt() + // currentDepth
            this->entryInfoPtr->serialLen() + // entryInfo
            Serialization::serialLenPath(path);
      }


   private:
      unsigned searchDepth;
      unsigned currentDepth;

      // for serialization
      Path* path; // not owned by this object!
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      PathDeserializationInfo pathDeserInfo;
      EntryInfo entryInfo;


   public:

      // inliners
      void parsePath(Path* outPath)
      {
         Serialization::deserializePath(pathDeserInfo, outPath);
      }

      // getters & setters
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

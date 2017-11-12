#ifndef LISTDIRFROMPFFSETRESPMSG_H_
#define LISTDIRFROMPFFSETRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class ListDirFromOffsetRespMsg : public NetMessage
{
   public:
      /**
       * @result FhgfsOpsErr
       * @param entryTypes can be an empty list (depending on whether this information was
       * requested with the ListDirFromOffsetMsg), but may not be NULL; contains DirEntryType values
       */
      ListDirFromOffsetRespMsg(int result, StringList* names, IntList* entryTypes,
         int64_t newServerOffset) :
         NetMessage(NETMSGTYPE_ListDirFromOffsetResp)
      {
         this->result = result;
         this->names = names;
         this->entryTypes = entryTypes;
         this->newServerOffset = newServerOffset;
      }

      ListDirFromOffsetRespMsg() : NetMessage(NETMSGTYPE_ListDirFromOffsetResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            Serialization::serialLenStringList(names) +
            Serialization::serialLenIntList(entryTypes) +
            Serialization::serialLenInt64(); // newServerOffset
      }
      
   private:
      int result;
      
      int64_t newServerOffset;

      // for serialization
      StringList* names; // not owned by this object!
      IntList* entryTypes; // not owned by this object!
      
      // for deserialization
      unsigned namesElemNum;
      const char* namesListStart;
      unsigned namesBufLen;
      unsigned entryTypesElemNum;
      const char* entryTypesListStart;
      unsigned entryTypesBufLen;

      
   public:
      // inliners   
      bool parseNames(StringList* outNames)
      {
         return Serialization::deserializeStringList(
            namesBufLen, namesElemNum, namesListStart, outNames);
      }
      
      bool parseEntryTypes(IntList* outEntryTypes)
      {
         return Serialization::deserializeIntList(
            entryTypesBufLen, entryTypesElemNum, entryTypesListStart, outEntryTypes);
      }

      // getters & setters
      int getResult()
      {
         return result;
      }
      
      int64_t getNewServerOffset()
      {
         return newServerOffset;
      }
   
};

#endif /*LISTDIRFROMPFFSETRESPMSG_H_*/

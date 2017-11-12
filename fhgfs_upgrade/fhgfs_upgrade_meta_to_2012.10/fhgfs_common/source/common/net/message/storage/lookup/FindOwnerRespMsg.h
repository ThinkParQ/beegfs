#ifndef FINDOWNERRESPMSG_H_
#define FINDOWNERRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/EntryOwnerInfo.h>

/*
 * Serialization is a bit tricky here. EntryInfo has its own serilization methods and as we do not
 * want to have code duplication into EntryOwnerInfo, we convert EntryOwnerInfo to EntryInfo,
 * and handle the only missing member 'entryDepth' separately.
 */


class FindOwnerRespMsg : public NetMessage
{
   public:

      /**
       * @param ownerNodeID just a reference, so do not free it as long as you use this object!
       * @param parentEntryID just a reference, so do not free it as long as you use this object
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      FindOwnerRespMsg(int result, EntryOwnerInfo* entryOwnerInfo) :
         NetMessage(NETMSGTYPE_FindOwnerResp)
      {
         this->result = result;

         this->entryDepth = entryOwnerInfo->getEntryDepth();
         this->entryInfo.updateFromOwnerInfo(entryOwnerInfo);
      }

      FindOwnerRespMsg() : NetMessage(NETMSGTYPE_FindOwnerResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            Serialization::serialLenUInt() + // entryDepth
            this->entryInfo.serialLen(); // entryInfo
      }


   private:
      int result;

      unsigned entryDepth;
      EntryInfo entryInfo;

   public:

      // inliners

      // getters & setters
      int getResult()
      {
         return result;
      }

      unsigned getOwnerDepth()
      {
         return entryDepth;
      }

      EntryInfo *getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      void getOwnerInfo(EntryOwnerInfo* outEntryInfo)
      {
         this->entryInfo.getOwnerInfo(this->entryDepth, outEntryInfo);
      }

};

#endif /*FINDOWNERRESPMSG_H_*/

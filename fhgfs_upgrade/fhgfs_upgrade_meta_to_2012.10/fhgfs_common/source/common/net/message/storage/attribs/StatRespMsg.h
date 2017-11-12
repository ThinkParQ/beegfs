#ifndef STATRESPMSG_H_
#define STATRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>
#include <common/storage/Metadata.h>

class StatRespMsg : public NetMessage
{
   public:
      StatRespMsg(int result, StatData statData) :
         NetMessage(NETMSGTYPE_StatResp)
      {

         this->result = result;
         this->statData = statData;
      }

      StatRespMsg() : NetMessage(NETMSGTYPE_StatResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            this->statData.serialLen();     // statData
      }
      
   private:
      int result;
      StatData statData;

   public:
      // getters & setters
      int getResult()
      {
         return result;
      }

      StatData* getStatData()
      {
         return &this->statData;
      }
};

#endif /*STATRESPMSG_H_*/

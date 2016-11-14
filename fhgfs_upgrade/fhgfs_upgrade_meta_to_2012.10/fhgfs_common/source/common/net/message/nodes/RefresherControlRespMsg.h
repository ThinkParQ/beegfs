#ifndef REFRESHERCONTROLRESPMSG_H_
#define REFRESHERCONTROLRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>


class RefresherControlRespMsg : public NetMessage
{
   public:
      RefresherControlRespMsg(bool isRunning, uint64_t numDirsRefreshed,
         uint64_t numFilesRefreshed) : NetMessage(NETMSGTYPE_RefresherControlResp)
      {
         this->isRunning = isRunning;
         this->numDirsRefreshed = numDirsRefreshed;
         this->numFilesRefreshed = numFilesRefreshed;
      }

      /**
       * Constructor for deserialization only!
       */
      RefresherControlRespMsg() : NetMessage(NETMSGTYPE_RefresherControlResp)
      {
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenBool() + // isRunning
            Serialization::serialLenUInt64() + // numDirsRefreshed
            Serialization::serialLenUInt64(); // numFilesRefreshed
      }


   private:
      bool isRunning;
      uint64_t numDirsRefreshed;
      uint64_t numFilesRefreshed;


   public:

      // getters & setters
      bool getIsRunning() const
      {
         return isRunning;
      }

      uint64_t getNumDirsRefreshed() const
      {
         return numDirsRefreshed;
      }

      uint64_t getNumFilesRefreshed() const
      {
         return numFilesRefreshed;
      }

};


#endif /* REFRESHERCONTROLRESPMSG_H_ */

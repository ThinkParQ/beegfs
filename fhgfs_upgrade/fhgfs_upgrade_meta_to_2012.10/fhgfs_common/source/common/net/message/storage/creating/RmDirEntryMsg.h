#ifndef RMDIRENTRYMSG_H_
#define RMDIRENTRYMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>


class RmDirEntryMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       */
      RmDirEntryMsg(Path* path) : NetMessage(NETMSGTYPE_RmDirEntry)
      {
         this->path = path;
      }


   protected:
      RmDirEntryMsg() : NetMessage(NETMSGTYPE_RmDirEntry)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH + Serialization::serialLenPath(path);
      }


   private:
      // for serialization
      Path* path; // not owned by this object!

      // for deserialization
      PathDeserializationInfo pathDeserInfo;


   public:

      // inliners
      void parsePath(Path* outPath)
      {
         Serialization::deserializePath(pathDeserInfo, outPath);
      }

      // getters & setters

};


#endif /* RMDIRENTRYMSG_H_ */

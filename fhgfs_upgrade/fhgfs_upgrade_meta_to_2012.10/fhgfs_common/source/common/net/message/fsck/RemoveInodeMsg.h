#ifndef REMOVEINODEMSG_H
#define REMOVEINODEMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/toolkit/FsckTk.h>


class RemoveInodeMsg : public NetMessage
{
   public:
      
      RemoveInodeMsg(std::string& entryID, DirEntryType entryType) :
         NetMessage(NETMSGTYPE_RemoveInode)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();
         this->entryType = entryType;
      }

   protected:
      RemoveInodeMsg() : NetMessage(NETMSGTYPE_RemoveInode)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
         Serialization::serialLenStr(entryIDLen) +
         Serialization::serialLenInt();
      }


   private:
      unsigned entryIDLen;
      const char* entryID;
      DirEntryType entryType;

   public:

      // getters & setters

      std::string getEntryID()
      {
         return std::string(entryID);
      }

      DirEntryType getEntryType()
      {
         return entryType;
      }

};


#endif /*REMOVEINODEMSG_H*/

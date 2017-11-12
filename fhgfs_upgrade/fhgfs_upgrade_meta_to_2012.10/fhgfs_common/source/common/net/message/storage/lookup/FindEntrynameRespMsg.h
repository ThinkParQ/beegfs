#ifndef FindEntrynameRESPMSG_H
#define FindEntrynameRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class FindEntrynameRespMsg: public NetMessage
{
   public:
      FindEntrynameRespMsg(bool entriesLeft, std::string& entryName, std::string& parentID,
         long currentDirLoc, long lastEntryLoc) :
         NetMessage(NETMSGTYPE_FindEntrynameResp)
      {
         this->entriesLeft = entriesLeft;
         this->entryName = entryName.c_str();
         this->entryNameLen = strlen(this->entryName);
         this->parentID = parentID.c_str();
         this->parentIDLen = strlen(this->parentID);
         this->currentDirLoc = currentDirLoc;
         this->lastEntryLoc = lastEntryLoc;
      }

      FindEntrynameRespMsg() :
         NetMessage(NETMSGTYPE_FindEntrynameResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH + Serialization::serialLenBool()
            + Serialization::serialLenStr(entryNameLen) + Serialization::serialLenStr(parentIDLen)
            + Serialization::serialLenInt64() + Serialization::serialLenInt64();
      }

   private:
      const char *entryName;
      unsigned entryNameLen;
      const char *parentID;
      unsigned parentIDLen;
      bool entriesLeft;
      int64_t currentDirLoc;
      int64_t lastEntryLoc;

   public:

      // getters & setters
      std::string getEntryName()
      {
         return std::string(entryName);
      }

      std::string getParentID()
      {
         return std::string(parentID);
      }

      int getEntriesLeft()
      {
         return entriesLeft;
      }

      long getCurrentDirLoc()
      {
         return currentDirLoc;
      }

      long getLastEntryLoc()
      {
         return lastEntryLoc;
      }

};

#endif /*FindEntrynameRESPMSG_H*/

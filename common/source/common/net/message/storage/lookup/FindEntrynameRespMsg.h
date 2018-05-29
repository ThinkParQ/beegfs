#ifndef FindEntrynameRESPMSG_H
#define FindEntrynameRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class FindEntrynameRespMsg: public NetMessageSerdes<FindEntrynameRespMsg>
{
   public:
      FindEntrynameRespMsg(bool entriesLeft, std::string& entryName, std::string& parentID,
         long currentDirLoc, long lastEntryLoc) :
         BaseType(NETMSGTYPE_FindEntrynameResp)
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
         BaseType(NETMSGTYPE_FindEntrynameResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->entriesLeft
            % serdes::rawString(obj->entryName, obj->entryNameLen)
            % serdes::rawString(obj->parentID, obj->parentIDLen)
            % obj->currentDirLoc
            % obj->lastEntryLoc;
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

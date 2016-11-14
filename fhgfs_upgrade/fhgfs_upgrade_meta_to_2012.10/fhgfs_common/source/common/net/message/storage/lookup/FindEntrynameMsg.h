#ifndef FindEntrynameMSG_H
#define FindEntrynameMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

/* Send a ID to MDS => response will be the name of the appropriate ID (if the MDS is the link
 * owner)
 *
 * we do not want to run into any timeouts, therefore we need to make it possible to check
 * incremental
 */

class FindEntrynameMsg : public NetMessage
{
   public:
      FindEntrynameMsg(std::string& id, long currentDirLoc,
         long lastEntryLoc) : NetMessage(NETMSGTYPE_FindEntryname)
      {
         this->id = id.c_str();
         this->idLen = strlen(this->id);
         this->dirId = "";
         this->dirIdLen = strlen(this->dirId);
         this->currentDirLoc = currentDirLoc;
         this->lastEntryLoc = lastEntryLoc;
      }

      //can be called with a suggested directory ID to look into
      FindEntrynameMsg(std::string& id, std::string& dirId) : NetMessage(NETMSGTYPE_FindEntryname)
      {
         this->id = id.c_str();
         this->idLen = strlen(this->id);
         this->dirId = dirId.c_str();
         this->dirIdLen = strlen(this->dirId);
         this->currentDirLoc = 0;
         this->lastEntryLoc = 0;
      }


   protected:
      FindEntrynameMsg() : NetMessage(NETMSGTYPE_FindEntryname)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
             Serialization::serialLenStr(idLen) +
             Serialization::serialLenStr(dirIdLen) +
             Serialization::serialLenInt64() +
             Serialization::serialLenInt64();
      }


   private:
      const char* id;
      unsigned idLen;
      const char* dirId;
      unsigned dirIdLen;
      int64_t currentDirLoc;
      int64_t lastEntryLoc;

   public:

      // getters & setters
      std::string getID()
      {
         return std::string(id);
      }

      std::string getDirID()
      {
         return std::string(dirId);
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


#endif /*FindEntrynameMSG_H*/

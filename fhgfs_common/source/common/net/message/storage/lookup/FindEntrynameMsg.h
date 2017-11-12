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

class FindEntrynameMsg : public NetMessageSerdes<FindEntrynameMsg>
{
   public:
      FindEntrynameMsg(std::string& id, long currentDirLoc,
         long lastEntryLoc) : BaseType(NETMSGTYPE_FindEntryname)
      {
         this->id = id.c_str();
         this->idLen = strlen(this->id);
         this->dirId = "";
         this->dirIdLen = strlen(this->dirId);
         this->currentDirLoc = currentDirLoc;
         this->lastEntryLoc = lastEntryLoc;
      }

      //can be called with a suggested directory ID to look into
      FindEntrynameMsg(std::string& id, std::string& dirId) : BaseType(NETMSGTYPE_FindEntryname)
      {
         this->id = id.c_str();
         this->idLen = strlen(this->id);
         this->dirId = dirId.c_str();
         this->dirIdLen = strlen(this->dirId);
         this->currentDirLoc = 0;
         this->lastEntryLoc = 0;
      }

      FindEntrynameMsg() : BaseType(NETMSGTYPE_FindEntryname)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->id, obj->idLen)
            % serdes::rawString(obj->dirId, obj->dirIdLen)
            % obj->currentDirLoc
            % obj->lastEntryLoc;
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

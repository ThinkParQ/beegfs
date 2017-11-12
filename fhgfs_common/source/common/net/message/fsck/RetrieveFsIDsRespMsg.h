#ifndef RETRIEVEFSIDSRESPMSG_H
#define RETRIEVEFSIDSRESPMSG_H

#include <common/Common.h>
#include <common/fsck/FsckFsID.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/serialization/Serialization.h>

class RetrieveFsIDsRespMsg : public NetMessageSerdes<RetrieveFsIDsRespMsg>
{
   public:
      RetrieveFsIDsRespMsg(FsckFsIDList* fsckFsIDs, std::string& currentContDirID,
         int64_t newHashDirOffset, int64_t newContDirOffset) :
            BaseType(NETMSGTYPE_RetrieveFsIDsResp)
      {
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->newHashDirOffset = newHashDirOffset;
         this->newContDirOffset = newContDirOffset;
         this->fsckFsIDs = fsckFsIDs;
      }

      RetrieveFsIDsRespMsg() : BaseType(NETMSGTYPE_RetrieveFsIDsResp)
      {
      }

   private:
      const char* currentContDirID;
      unsigned currentContDirIDLen;
      int64_t newHashDirOffset;
      int64_t newContDirOffset;

      FsckFsIDList* fsckFsIDs;

      // for deserialization
      struct {
         FsckFsIDList fsckFsIDs;
      } parsed;

   public:
      FsckFsIDList& getFsIDs()
      {
         return *fsckFsIDs;
      }

      int64_t getNewHashDirOffset() const
      {
         return newHashDirOffset;
      }

      int64_t getNewContDirOffset() const
      {
         return newContDirOffset;
      }

      std::string getCurrentContDirID() const
      {
         return currentContDirID;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->currentContDirID, obj->currentContDirIDLen, 4)
            % obj->newHashDirOffset
            % obj->newContDirOffset
            % serdes::backedPtr(obj->fsckFsIDs, obj->parsed.fsckFsIDs);
      }
};

#endif /*RETRIEVEFSIDSRESPMSG_H*/

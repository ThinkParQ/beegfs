#ifndef MOVECHUNKFILESMSG_H
#define MOVECHUNKFILESMSG_H

#include <common/net/message/NetMessage.h>

class MoveChunkFileMsg: public NetMessageSerdes<MoveChunkFileMsg>
{
   public:
      /*
       * @param chunkName the name of the chunk (i.e. the "ID")
       * @param targetID
       * @param isMirrored targetID describes a buddy group
       * @param oldPath relative to storeStorageDirectory and mirror chunk Path
       * @param newPath relative to storeStorageDirectory and mirror chunk Path
       * @param overwriteExisting if true, the destination file will be overwritten if it exists
       */
      MoveChunkFileMsg(const std::string& chunkName, uint16_t targetID,
            bool isMirrored, const std::string& oldPath, const std::string& newPath,
            bool overwriteExisting = false):
         BaseType(NETMSGTYPE_MoveChunkFile),
         chunkName(chunkName), oldPath(oldPath), newPath(newPath), targetID(targetID),
         isMirrored(isMirrored), overwriteExisting(overwriteExisting)
      {
      }

      MoveChunkFileMsg() : BaseType(NETMSGTYPE_MoveChunkFile)
      {
      }

   private:
      std::string chunkName;
      std::string oldPath;
      std::string newPath;

      uint16_t targetID;
      bool isMirrored;

      bool overwriteExisting;

   public:
      std::string getChunkName() const
      {
         return chunkName;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      bool getIsMirrored() const
      {
         return isMirrored;
      }

      std::string getOldPath() const
      {
         return oldPath;
      }

      std::string getNewPath() const
      {
         return newPath;
      }

      bool getOverwriteExisting() const
      {
         return overwriteExisting;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->chunkName
            % obj->oldPath
            % obj->newPath
            % obj->targetID
            % obj->isMirrored
            % obj->overwriteExisting;
      }
};

#endif /*MOVECHUNKFILESMSG_H*/

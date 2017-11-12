#ifndef LISTCHUNKDIRINCREMENTALMSG_H_
#define LISTCHUNKDIRINCREMENTALMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/net/message/storage/listing/ListChunkDirIncrementalRespMsg.h>

class ListChunkDirIncrementalMsg : public NetMessageSerdes<ListChunkDirIncrementalMsg>
{
   public:
      ListChunkDirIncrementalMsg(uint16_t targetID, bool isMirror, const std::string& relativeDir,
            int64_t offset, unsigned maxOutEntries, bool onlyFiles, bool ignoreNotExists) :
         BaseType(NETMSGTYPE_ListChunkDirIncremental), targetID(targetID), isMirror(isMirror),
         relativeDir(relativeDir), offset(offset), maxOutEntries(maxOutEntries),
         onlyFiles(onlyFiles), ignoreNotExists(ignoreNotExists) { }

      /**
       * For deserialization only
       */
      ListChunkDirIncrementalMsg() : BaseType(NETMSGTYPE_ListChunkDirIncremental) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->relativeDir)
            % obj->targetID
            % obj->isMirror
            % obj->offset
            % obj->maxOutEntries
            % obj->onlyFiles
            % obj->ignoreNotExists;
      }

   private:
      uint16_t targetID;
      bool isMirror;
      std::string relativeDir;
      int64_t offset;
      uint32_t maxOutEntries;
      bool onlyFiles;
      bool ignoreNotExists;

   public:
      uint16_t getTargetID() const { return targetID; }
      bool getIsMirror() const { return isMirror; }
      int64_t getOffset() const { return offset; }
      unsigned getMaxOutEntries() const { return maxOutEntries; }
      std::string getRelativeDir() const { return relativeDir; }
      bool getOnlyFiles() const { return onlyFiles; }
      bool getIgnoreNotExists() const { return ignoreNotExists; }
};


#endif /*LISTCHUNKDIRINCREMENTALMSG_H_*/

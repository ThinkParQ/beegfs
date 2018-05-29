#ifndef META_RESYNCRAWINODESMSGEX_H
#define META_RESYNCRAWINODESMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <storage/IncompleteInode.h>

class ResyncRawInodesMsgEx : public NetMessageSerdes<ResyncRawInodesMsgEx>
{
   public:
      ResyncRawInodesMsgEx(Path basePath, bool hasXAttrs, bool wholeDirectory):
         BaseType(NETMSGTYPE_ResyncRawInodes),
         basePath(std::move(basePath)),
         hasXAttrs(hasXAttrs),
         wholeDirectory(wholeDirectory)
      {}

      ResyncRawInodesMsgEx(): BaseType(NETMSGTYPE_ResyncRawInodes) {}

      bool processIncoming(ResponseContext& ctx) override;

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->basePath
            % obj->hasXAttrs
            % obj->wholeDirectory;
      }

   private:
      Path basePath;
      bool hasXAttrs;
      bool wholeDirectory;

      std::vector<std::string> inodesWritten;

      FhgfsOpsErr resyncStream(ResponseContext& ctx);

      FhgfsOpsErr resyncSingle(ResponseContext& ctx);

      FhgfsOpsErr resyncInode(ResponseContext& ctx, const Path& path, Deserializer& data,
         const bool isDirectory, const bool recvXAttrs = true);
      FhgfsOpsErr resyncDentry(ResponseContext& ctx, const Path& path, Deserializer& data);

      FhgfsOpsErr resyncInodeXAttrs(ResponseContext& ctx, IncompleteInode& inode);

      FhgfsOpsErr removeUntouchedInodes();
};

#endif /* RESYNCRAWDENTRYMSGEX_H_ */

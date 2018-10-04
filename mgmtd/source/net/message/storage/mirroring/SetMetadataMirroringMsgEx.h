#ifndef SETMETADATAMIRRORINGMSGEX_H_
#define SETMETADATAMIRRORINGMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringMsg.h>

class App;

class SetMetadataMirroringMsgEx : public SetMetadataMirroringMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr process(App& app);
};

#endif

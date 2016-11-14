#ifndef LOOKUPINTENTMSGEX_H_
#define LOOKUPINTENTMSGEX_H_

#include <common/net/message/storage/lookup/LookupIntentMsg.h>
#include <common/nodes/OpCounterTypes.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include <storage/MetaStore.h>


/**
 * This combines the normal lookup of a directory entry with intents, i.e. options to create,
 * open and stat the entry in a single message.
 *
 * Note: The intent options currently work only for files.
 */
class LookupIntentMsgEx : public LookupIntentMsg
{
   public:
      LookupIntentMsgEx() : LookupIntentMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

   protected:

   private:
      FhgfsOpsErr lookup(std::string& parentEntryID, std::string& entryName,
         EntryInfo* outEntryInfo, DentryInodeMeta* outInodeMetaData);
      FhgfsOpsErr revalidate(EntryInfo* entryInfo);
      FhgfsOpsErr create(EntryInfo* parentInfo, std::string& entryName, EntryInfo* outEntryInfo,
         DentryInodeMeta* outInodeData);
      FhgfsOpsErr stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData);
      FhgfsOpsErr open(EntryInfo* entryInfo, std::string* outFileHandleID,
         StripePattern** outPattern);

      MetaOpCounterTypes getOpCounterType();

};



#endif /* LOOKUPINTENTMSGEX_H_ */

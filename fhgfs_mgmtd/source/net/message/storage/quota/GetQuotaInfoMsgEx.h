#ifndef GETQUOTAINFOMSGEX_H_
#define GETQUOTAINFOMSGEX_H_


#include <common/net/message/storage/quota/GetQuotaInfoMsg.h>
#include <common/storage/quota/QuotaData.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/Common.h>
#include <components/quota/QuotaManager.h>



class GetQuotaInfoMsgEx : public GetQuotaInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      bool requestQuotaForID(unsigned id, QuotaDataType type, const QuotaManager* quotaManager,
         QuotaDataList& outQuotaDataList);
      bool requestQuotaForRange(const QuotaManager* quotaManager, QuotaDataList& outQuotaDataList);
      bool requestQuotaForList(const QuotaManager* quotaManager, QuotaDataList& outQuotaDataList);
};

#endif /* GETQUOTAINFOMSGEX_H_ */

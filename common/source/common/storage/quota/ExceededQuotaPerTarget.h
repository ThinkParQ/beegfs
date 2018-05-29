#ifndef COMMON_NEXCEEDEDQUOTAPERTARGET_H_
#define COMMON_NEXCEEDEDQUOTAPERTARGET_H_

#include <common/storage/quota/ExceededQuotaStore.h>

class ExceededQuotaPerTarget
{
   public:
      const ExceededQuotaStorePtr get(uint16_t targetId) const;
      const ExceededQuotaStorePtr add(uint16_t targetId, bool ignoreExisting = true);
      void remove(uint16_t targetId);

   private:
      // key: targetId, value: shared ptr to ExceededQuotaStore
      std::map<uint16_t, ExceededQuotaStorePtr> elements;
      mutable RWLock rwLock;
};

#endif /* COMMON_NEXCEEDEDQUOTAPERTARGET_H_ */

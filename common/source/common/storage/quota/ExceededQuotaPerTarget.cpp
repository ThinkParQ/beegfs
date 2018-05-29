#include "ExceededQuotaPerTarget.h"

#include <common/threading/RWLockGuard.h>

const ExceededQuotaStorePtr ExceededQuotaPerTarget::get(uint16_t targetId) const
{
   RWLockGuard lock(rwLock, SafeRWLock_READ);

   auto iter = elements.find(targetId);

   if (iter != elements.end())
      return iter->second;
   else
      return {};
}

/*
 * add an exceededQuotaStore for a specific target
 *
 * @param targetID
 * @param ignoreExisting do not log error if store already exists
 */
const ExceededQuotaStorePtr ExceededQuotaPerTarget::add(uint16_t targetId,
   bool ignoreExisting)
{
   ExceededQuotaStorePtr exStore = std::make_shared<ExceededQuotaStore>();

   RWLockGuard lock(rwLock, SafeRWLock_WRITE);

   auto insertRes = elements.insert(std::make_pair(targetId, exStore));

   if ((!ignoreExisting) && (!insertRes.second))
      LOG(QUOTA, NOTICE, "Tried to add exceeded quota store, which already exists.", targetId);

   return (insertRes.first)->second;
}

/*
 * remove an exceededQuotaStore of a specific target
 *
 * @param targetID
 */
void ExceededQuotaPerTarget::remove(uint16_t targetId)
{
   RWLockGuard lock(rwLock, SafeRWLock_WRITE);

   elements.erase(targetId);
}

#include "ExceededQuotaStore.h"


#include <common/threading/SafeRWLock.h>


/**
 * Replace the old list of IDs with exceeded quota by the new List of IDs with exceeded quota
 *
 * @param newIDList the old list with the IDs with exceeded quota (requires a list which was
 *        allocated with new)
 * @param idType the type of the ID list (user or group)
 * @param exType the type of exceeded quota (size or inodes)
 */
void ExceededQuotaStore::updateExceededQuota(UIntList* newIDList, QuotaDataType idType,
   QuotaLimitType exType)
{
   SafeRWLock lock(&this->rwLockExceededLists, SafeRWLock_WRITE);             //W R I T E L O C K

   if(idType == QuotaDataType_USER)
   {
      if(exType == QuotaLimitType_SIZE)
         updateExceededQuotaUnlocked(newIDList, this->exceededUserQuotaSize);
      else
      if(exType == QuotaLimitType_INODE)
         updateExceededQuotaUnlocked(newIDList, this->exceededUserQuotaInode);
   }
   else
   if(idType == QuotaDataType_GROUP)
   {
      if(exType == QuotaLimitType_SIZE)
         updateExceededQuotaUnlocked(newIDList, this->exceededGroupQuotaSize);
      else
      if(exType == QuotaLimitType_INODE)
         updateExceededQuotaUnlocked(newIDList, this->exceededGroupQuotaInode);
   }

   uint64_t sum = this->exceededUserQuotaSize->size() +
                  this->exceededUserQuotaInode->size() +
                  this->exceededGroupQuotaSize->size() +
                  this->exceededGroupQuotaInode->size();

   this->exceededCounter.set(sum);

   lock.unlock();                                                             //U N L O C K
}

/**
 * Checks if the quota (size and inodes) is exceeded for the given IDs
 *
 * @param uid the user ID to check
 * @param gid the group ID to check
 *
 * @return the kind of exceeded quota, enum QuotaExceededErrorType
 */
QuotaExceededErrorType ExceededQuotaStore::isQuotaExceeded(unsigned uid, unsigned gid)
{
   QuotaExceededErrorType retVal = QuotaExceededErrorType_NOT_EXCEEDED;

   SafeRWLock lock(&this->rwLockExceededLists, SafeRWLock_READ);                 // R E A D L O C K

   if(this->isQuotaExceededUnlocked(uid, this->exceededUserQuotaSize) )
      retVal = QuotaExceededErrorType_USER_SIZE_EXCEEDED;
   else
   if(this->isQuotaExceededUnlocked(uid, this->exceededUserQuotaInode) )
      retVal = QuotaExceededErrorType_USER_INODE_EXCEEDED;
   else
   if(this->isQuotaExceededUnlocked(gid, this->exceededGroupQuotaSize) )
      retVal = QuotaExceededErrorType_GROUP_SIZE_EXCEEDED;
   else
   if(this->isQuotaExceededUnlocked(gid, this->exceededGroupQuotaInode) )
      retVal = QuotaExceededErrorType_GROUP_INODE_EXCEEDED;

   lock.unlock();                                                                // U N L O C K

   return retVal;
}

/**
 * Checks if the quota is exceeded for the given IDs of the given exceeded type (size or inodes)
 *
 * @param uid the user ID to check
 * @param gid the group ID to check
 * @param exType the type of exceeded quota (size or inodes)
 *
 * @return the kind of exceeded quota, enum QuotaExceededErrorType
 */
QuotaExceededErrorType ExceededQuotaStore::isQuotaExceeded(unsigned uid, unsigned gid,
   QuotaLimitType exType)
{
   QuotaExceededErrorType retVal = QuotaExceededErrorType_NOT_EXCEEDED;

   SafeRWLock lock(&this->rwLockExceededLists, SafeRWLock_READ);                 // R E A D L O C K

   if(exType == QuotaLimitType_SIZE)
   {
      if(this->isQuotaExceededUnlocked(uid, this->exceededUserQuotaSize) )
         retVal = QuotaExceededErrorType_USER_SIZE_EXCEEDED;
      else
      if(this->isQuotaExceededUnlocked(gid, this->exceededGroupQuotaSize) )
         retVal = QuotaExceededErrorType_GROUP_SIZE_EXCEEDED;
   }
   else
   if(exType == QuotaLimitType_INODE)
   {
      if(this->isQuotaExceededUnlocked(uid, this->exceededUserQuotaInode) )
         retVal = QuotaExceededErrorType_USER_INODE_EXCEEDED;
      else
      if(this->isQuotaExceededUnlocked(gid, this->exceededGroupQuotaInode) )
         retVal = QuotaExceededErrorType_GROUP_INODE_EXCEEDED;
   }

   lock.unlock();                                                                // U N L O C K

   return retVal;
}

/**
 * Collects the IDs with exceeded quota of the given types
 *
 * @param outIDList the list with the IDs of exceeded quota
 * @param idType the type of the ID list (user or group)
 * @param exType the type of exceeded quota (size or inodes)
 */
void ExceededQuotaStore::getExceededQuota(UIntList* outIDList, QuotaDataType idType,
   QuotaLimitType exType) const
{
   SafeRWLock lock(&this->rwLockExceededLists, SafeRWLock_READ);         // R E A D L O C K

   if(idType == QuotaDataType_USER)
   {
      if(exType == QuotaLimitType_SIZE)
         this->getExceededQuotaUnlocked(outIDList, this->exceededUserQuotaSize);
      else
      if(exType == QuotaLimitType_INODE)
         this->getExceededQuotaUnlocked(outIDList, this->exceededUserQuotaInode);
   }
   else
   if(idType == QuotaDataType_GROUP)
   {
      if(exType == QuotaLimitType_SIZE)
         this->getExceededQuotaUnlocked(outIDList, this->exceededGroupQuotaSize);
      else
      if(exType == QuotaLimitType_INODE)
         this->getExceededQuotaUnlocked(outIDList, this->exceededGroupQuotaInode);
   }

   lock.unlock();                                                                // U N L O C K
}

/**
 * Replace the old list of IDs with exceeded quota by the new List of IDs with exceeded quota, not
 * synchronised
 *
 * @param newIDList the old list with the IDs with exceeded quota (requires a list which was
 *        allocated with new)
 * @param listToUpdate the old list with the IDs with exceeded quota
 */
void ExceededQuotaStore::updateExceededQuotaUnlocked(UIntList* newIDList, UIntSet* listToUpdate)
{
   listToUpdate->clear();

   for(UIntListIter iter = newIDList->begin(); iter != newIDList->end(); iter++)
      listToUpdate->insert(*iter);
}

/**
 * Checks if the quota is exceeded for the given ID, not synchronised
 *
 * @param id the ID to check
 * @param list the list of exceeded quota which could contain the given ID
 *
 * return true, if the quota is exceeded for the given id
 */
bool ExceededQuotaStore::isQuotaExceededUnlocked(unsigned id, UIntSet* list)
{
   bool retVal = false;

   for(UIntSetIter iter = list->begin(); iter != list->end(); iter++)
   {
      if(*iter == id)
      {
         retVal = true;
         break;
      }
   }

   return retVal;
}

/**
 * Collects the IDs with exceeded quota, not synchronised
 *
 * @param outIDList the out list with the IDs of exceeded quota
 * @param inIDList the in list with the  IDs of exceeded quota
 */
void ExceededQuotaStore::getExceededQuotaUnlocked(UIntList* outIDList, UIntSet* inIDList) const
{
   for(UIntSetIter iter = inIDList->begin(); iter != inIDList->end(); iter++)
      outIDList->push_back(*iter);
}

/**
 * Checks if the quota of at least one user or group is exceeded.
 *
 * @return true if at least one exceeded quota ID exists
 */
bool ExceededQuotaStore::someQuotaExceeded()
{
   return this->exceededCounter.read() != 0;
}

#include "StoragePoolEx.h"

StoragePoolEx::StoragePoolEx(StoragePoolId id, const std::string& description,
   const DynamicPoolLimits& capPoolLimitsStorageSpace,
   const DynamicPoolLimits& capPoolLimitsStorageInodes, bool useDynamicCapPools) :
   StoragePool(id, description, capPoolLimitsStorageSpace, capPoolLimitsStorageInodes,
               useDynamicCapPools)
{
   initQuota();
};

void StoragePoolEx::initQuota()
{
   quotaUserLimits = std::unique_ptr<QuotaStoreLimits>(new QuotaStoreLimits(
         QuotaDataType_USER,
         "QuotaUserLimits (Pool: " + id.str() + ")",
         USER_LIMITS_STORE_PATH(id.str())));


   quotaGroupLimits = std::unique_ptr<QuotaStoreLimits>(new QuotaStoreLimits(
         QuotaDataType_GROUP,
         "QuotaGroupLimits (Pool: " + id.str() + ")",
         GROUP_LIMITS_STORE_PATH(id.str())));

   quotaDefaultLimits = std::unique_ptr<QuotaDefaultLimits>(new QuotaDefaultLimits(
         DEFAULT_LIMITS_STORE_PATH(id.str())));
}

void StoragePoolEx::clearQuota()
{
   if (quotaUserLimits)
      quotaUserLimits->clearLimits();

   if (quotaGroupLimits)
      quotaGroupLimits->clearLimits();

   if (quotaDefaultLimits)
      quotaDefaultLimits->clearLimits();
}

/*
 * wrapper around deserializer to perform additional tasks after deserialization in inherited
 * classes (in this case quota data initialization)
 */
bool StoragePoolEx::initFromDesBuf(Deserializer& des)
{
   // call init of base class
   bool desRes = StoragePool::initFromDesBuf(des);

   if (!desRes)
      return false;

   // additionally init quota structures
   initQuota();

   return true;
}

void StoragePoolEx::saveQuotaLimits()
{  // Note: no synchronization needed here; all done inside the Limits classes
   quotaUserLimits->saveToFile();
   quotaGroupLimits->saveToFile();
   quotaDefaultLimits->saveToFile();
}

void StoragePoolEx::loadQuotaLimits()
{  // Note: no synchronization needed here; all done inside the Limits classes
   quotaUserLimits->loadFromFile();
   quotaGroupLimits->loadFromFile();
   quotaDefaultLimits->loadFromFile();
}

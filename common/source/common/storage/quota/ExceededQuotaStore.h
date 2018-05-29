#ifndef EXCEEDEDQUOTASTORE_H_
#define EXCEEDEDQUOTASTORE_H_


#include <common/Common.h>
#include <common/storage/quota/QuotaData.h>
#include <common/threading/RWLock.h>

class ExceededQuotaStore
{
   public:
      ExceededQuotaStore()
      {
         this->exceededUserQuotaSize = new UIntSet();
         this->exceededGroupQuotaSize = new UIntSet();
         this->exceededUserQuotaInode = new UIntSet();
         this->exceededGroupQuotaInode = new UIntSet();
      }

      ~ExceededQuotaStore()
      {
         SAFE_DELETE(this->exceededUserQuotaSize);
         SAFE_DELETE(this->exceededGroupQuotaSize);
         SAFE_DELETE(this->exceededUserQuotaInode);
         SAFE_DELETE(this->exceededGroupQuotaInode);
      }

      void updateExceededQuota(UIntList* newIDList, QuotaDataType idType, QuotaLimitType exType);
      QuotaExceededErrorType isQuotaExceeded(unsigned uid, unsigned gid, QuotaLimitType exType);
      QuotaExceededErrorType isQuotaExceeded(unsigned uid, unsigned gid);
      void getExceededQuota(UIntList* outIDList, QuotaDataType idType, QuotaLimitType exType) const;
      bool someQuotaExceeded();


   private:
      UIntSet* exceededUserQuotaSize;      // UIDs with exceeded size quota
      UIntSet* exceededGroupQuotaSize;     // GIDs with exceeded inode quota
      UIntSet* exceededUserQuotaInode;     // UIDs with exceeded size quota
      UIntSet* exceededGroupQuotaInode;    // GIDs with exceeded inode quota

      mutable RWLock rwLockExceededLists;  // synchronize access to all exceeded ID sets
      AtomicUInt64 exceededCounter;        // number of exceeded quota IDs in all categories


      void updateExceededQuotaUnlocked(UIntList* newIDList, UIntSet* listToUpdate);
      bool isQuotaExceededUnlocked(unsigned id, UIntSet* list);
      void getExceededQuotaUnlocked(UIntList* outIDList, UIntSet* inIDList) const;
};

typedef std::shared_ptr<ExceededQuotaStore> ExceededQuotaStorePtr;

#endif /* EXCEEDEDQUOTASTORE_H_ */

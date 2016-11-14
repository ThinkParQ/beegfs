#ifndef QUOTADATAREQUESTOR_H_
#define QUOTADATAREQUESTOR_H_


#include <common/Common.h>
#include <common/storage/quota/GetQuotaInfo.h>



class QuotaDataRequestor;                                               //forward declaration

typedef std::list<QuotaDataRequestor> QuotaDataRequestorList;
typedef QuotaDataRequestorList::iterator QuotaDataRequestorListIter;
typedef QuotaDataRequestorList::const_iterator QuotaDataRequestorListConstIter;


/**
 * collects the quota data from the storage servers
 */
class QuotaDataRequestor : public GetQuotaInfo
{
   public:
      /**
       * Constructor for QuotaDataRequestor to collect all IDs of the given type.
       *
       * @param type The type of the quota data, a value of the enum QuotaDataType.
       * @param withSystemusersGroups True if also system users should be queried.
       */
      QuotaDataRequestor(QuotaDataType type, bool withSystemusersGroups) : GetQuotaInfo()
      {
         this->cfg.cfgTargetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET;
         this->cfg.cfgType = type;
         this->cfg.cfgUseAll = true;
         this->cfg.cfgWithSystemUsersGroups = withSystemusersGroups;
      }

      /**
       * Constructor for QuotaDataRequestor to collect a single IDs of the given type.
       *
       * @param type The type of the quota data, a value of the enum QuotaDataType.
       * @param id The ID to query.
       */
      QuotaDataRequestor(QuotaDataType type, unsigned id) : GetQuotaInfo()
      {
         this->cfg.cfgTargetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET;
         this->cfg.cfgType = type;
         this->cfg.cfgID = id;
      }

      /**
       * Constructor for QuotaDataRequestor to collect all IDs from a IDs of the given type.
       * @param type The type of the quota data, a value of the enum QuotaDataType.
       * @param idList The ID list to query.
       */
      QuotaDataRequestor(QuotaDataType type, UIntList& idList) : GetQuotaInfo()
      {
         this->cfg.cfgTargetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET;
         this->cfg.cfgType = type;
         this->cfg.cfgUseList = true;
         this->cfg.cfgIDList = idList;
      }

      /**
       * Constructor for QuotaDataRequestor to collect IDs from a range of the given type.
       */
      QuotaDataRequestor(QuotaDataType type, unsigned rangeStart, unsigned rangeEnd,
         bool withSystemusersGroups) : GetQuotaInfo()
      {
         this->cfg.cfgTargetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET;
         this->cfg.cfgType = type;
         this->cfg.cfgUseRange = true;
         this->cfg.cfgIDRangeStart = rangeStart;
         this->cfg.cfgIDRangeEnd = rangeEnd;
         this->cfg.cfgWithSystemUsersGroups = withSystemusersGroups;
      }

      bool requestQuota(QuotaDataMapForTarget* outQuotaData, RWLock* outQuotaResultsRWLock,
         bool* isStoreDirty);

      /**
       * Updates the the targetID of the target to query. In this case only this target will be
       * queried.
       *
       * @param targetNumID The targetNumID to query.
       */
      void setTargetNumID(uint16_t targetNumID)
      {
         this->cfg.cfgTargetSelection = GETQUOTACONFIG_SINGLE_TARGET;
         this->cfg.cfgTargetNumID = targetNumID;
      }

   private:
      void updateQuotaDataWithResponse(QuotaDataMapForTarget* inQuotaData,
         QuotaDataMapForTarget* outQuotaData, TargetMapper* mapper);
};

#endif /* QUOTADATAREQUESTOR_H_ */

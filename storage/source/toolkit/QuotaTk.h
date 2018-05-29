#ifndef TOOLKIT_QUOTATK_H_
#define TOOLKIT_QUOTATK_H_



#include <common/Common.h>
#include <common/storage/quota/QuotaData.h>
#include <session/ZfsSession.h>
#include <storage/QuotaBlockDevice.h>


class QuotaTk
{
   public:
      /**
       * functions for extX and XFS support
       */
      static bool appendQuotaForID(unsigned id, QuotaDataType type,
         QuotaBlockDeviceMap* blockDevices, QuotaDataList* outQuotaDataList, ZfsSession* session);
      static bool requestQuotaForRange(QuotaBlockDeviceMap* blockDevices, unsigned rangeStart,
         unsigned rangeEnd, QuotaDataType type, QuotaDataList* outQuotaDataList,
         ZfsSession* session);
      static bool requestQuotaForList(QuotaBlockDeviceMap* blockDevices, UIntList* idList,
         QuotaDataType type, QuotaDataList* outQuotaDataList, ZfsSession* session);

      static bool checkQuota(QuotaBlockDeviceMap* blockDevices, QuotaData* outData,
         ZfsSession* session);


      /**
       * functions for ZFS support
       */
      static void* initLibZfs(void* dlHandle);
      static bool uninitLibZfs(void* dlHandle, void* libZfsHandle);
      static bool checkRequiredLibZfsFunctions(QuotaBlockDevice* blockDevice, uint16_t targetNumID);

      static bool requestQuotaFromZFS(QuotaBlockDevice* blockDevice, uint16_t targetNumID,
         QuotaData* outData, ZfsSession* session);

   private:
      QuotaTk();

};

#endif /* TOOLKIT_QUOTATK_H_ */

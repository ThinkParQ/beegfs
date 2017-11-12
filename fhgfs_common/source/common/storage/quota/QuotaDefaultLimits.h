#ifndef COMMON_QUOTADEFAULTLIMITS_H_
#define COMMON_QUOTADEFAULTLIMITS_H_

#include <common/toolkit/serialization/Serialization.h>
#include <string>

class QuotaDefaultLimits
{
   public:
      QuotaDefaultLimits():
            defaultUserQuotaInodes(0), defaultUserQuotaSize(0),
            defaultGroupQuotaInodes(0), defaultGroupQuotaSize(0)
      {};

      QuotaDefaultLimits(const std::string& storePath):
            storePath(storePath), defaultUserQuotaInodes(0), defaultUserQuotaSize(0),
            defaultGroupQuotaInodes(0), defaultGroupQuotaSize(0)
      {};

      QuotaDefaultLimits(const std::string& storePath, size_t userInodesLimit, size_t userSizeLimit,
         size_t groupInodesLimit, size_t groupSizeLimit):
            storePath(storePath), defaultUserQuotaInodes(userInodesLimit),
            defaultUserQuotaSize(userSizeLimit), defaultGroupQuotaInodes(groupInodesLimit),
            defaultGroupQuotaSize(groupSizeLimit)
      {};

   private:
      std::string storePath; // path on disk to store the data
      uint64_t defaultUserQuotaInodes;
      uint64_t defaultUserQuotaSize;
      uint64_t defaultGroupQuotaInodes;
      uint64_t defaultGroupQuotaSize;


   public:
      bool loadFromFile();
      bool saveToFile();

      void clearLimits();

      uint64_t getDefaultUserQuotaInodes() const
      {
         return defaultUserQuotaInodes;
      }

      uint64_t getDefaultUserQuotaSize() const
      {
         return defaultUserQuotaSize;
      }

      uint64_t getDefaultGroupQuotaInodes() const
      {
         return defaultGroupQuotaInodes;
      }

      uint64_t getDefaultGroupQuotaSize() const
      {
         return defaultGroupQuotaSize;
      }

      void updateUserLimits(uint64_t inodeLimit, uint64_t sizeLimit)
      {
         defaultUserQuotaInodes = inodeLimit;
         defaultUserQuotaSize = sizeLimit;
      }

      void updateGroupLimits(uint64_t inodeLimit, uint64_t sizeLimit)
      {
         defaultGroupQuotaInodes = inodeLimit;
         defaultGroupQuotaSize = sizeLimit;
      }

      void updateLimits(QuotaDefaultLimits& limits)
      {
         defaultUserQuotaInodes = limits.getDefaultUserQuotaInodes();
         defaultUserQuotaSize = limits.getDefaultUserQuotaSize();
         defaultGroupQuotaInodes = limits.getDefaultGroupQuotaInodes();
         defaultGroupQuotaSize = limits.getDefaultGroupQuotaSize();
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->defaultUserQuotaInodes
            % obj->defaultUserQuotaSize
            % obj->defaultGroupQuotaInodes
            % obj->defaultGroupQuotaSize;
      }

      unsigned serialLen() const
      {
         return serialize(NULL);
      }

      size_t serialize(char* buf) const
      {
         Serializer ser(buf, -1);
         ser % *this;
         return ser.size();
      }

      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen)
      {
         Deserializer des(buf, bufLen);
         des % *this;
         *outLen = des.size();
         return des.good();
      }
};

#endif /* COMMON_QUOTADEFAULTLIMITS_H_ */

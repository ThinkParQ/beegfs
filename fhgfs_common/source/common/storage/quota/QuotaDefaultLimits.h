#ifndef COMPONENTS_QUOTA_QUOTADEFAULTLIMITS_H_
#define COMPONENTS_QUOTA_QUOTADEFAULTLIMITS_H_



class QuotaDefaultLimits
{
   public:
      QuotaDefaultLimits() : defaultUserQuotaInodes(0), defaultUserQuotaSize(0),
         defaultGroupQuotaInodes(0), defaultGroupQuotaSize(0) {};
      QuotaDefaultLimits(size_t userInodesLimit, size_t userSizeLimit, size_t groupInodesLimit,
         size_t groupSizeLimit) : defaultUserQuotaInodes(userInodesLimit),
         defaultUserQuotaSize(userSizeLimit), defaultGroupQuotaInodes(groupInodesLimit),
         defaultGroupQuotaSize(groupSizeLimit) {};
      virtual ~QuotaDefaultLimits() {};


   private:
      uint64_t defaultUserQuotaInodes;
      uint64_t defaultUserQuotaSize;
      uint64_t defaultGroupQuotaInodes;
      uint64_t defaultGroupQuotaSize;


   public:
      uint64_t getDefaultUserQuotaInodes()
      {
         return defaultUserQuotaInodes;
      }

      uint64_t getDefaultUserQuotaSize()
      {
         return defaultUserQuotaSize;
      }

      uint64_t getDefaultGroupQuotaInodes()
      {
         return defaultGroupQuotaInodes;
      }

      uint64_t getDefaultGroupQuotaSize()
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

#endif /* COMPONENTS_QUOTA_QUOTADEFAULTLIMITS_H_ */

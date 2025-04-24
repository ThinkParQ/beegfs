#pragma once

#include <common/storage/StorageDefinitions.h>
#include <sstream>

class RemoteStorageTarget
{
   public:
      RemoteStorageTarget() {}

      RemoteStorageTarget(std::vector<uint32_t> rstIds) :
         RemoteStorageTarget(DEFAULT_COOLDOWN_PERIOD, DEFAULT_FILE_POLICY, rstIds)
      {
      }

      RemoteStorageTarget(uint16_t coolDownPeriod, uint16_t policyList,
         const std::vector<uint32_t>& rstIds) :
         majorVersion(DEFAULT_MAJOR_VER),
         minorVersion(DEFAULT_MINOR_VER),
         coolDownPeriod(coolDownPeriod), reserved(0), filePolicies(policyList), rstIdVec(rstIds)
      {   
      }

      void set(RemoteStorageTarget* other)
      {
         set(*other);
      }

      void set(const RemoteStorageTarget& other)
      {
         this->majorVersion = other.majorVersion;
         this->minorVersion = other.minorVersion;
         this->coolDownPeriod = other.coolDownPeriod;
         this->reserved = other.reserved;
         this->filePolicies = other.filePolicies;
         this->rstIdVec = other.rstIdVec;
      }

      // Reset all member variables to their initial/default state.
      // Effectively invalidates RST object by zeroing version and clearing all parameters.
      void reset()
      {
         majorVersion = 0;    // Invalidate RST object
         minorVersion = 0;
         coolDownPeriod = 0;
         reserved = 0;
         filePolicies = 0;
         rstIdVec.clear();    // Clear all remote target IDs
      }

      bool hasInvalidIds() const
      {
         return std::find(rstIdVec.begin(), rstIdVec.end(), 0) != rstIdVec.end();
      }

      bool hasDuplicateIds() const
      {
         std::set<uint32_t> uniqueIds(rstIdVec.begin(), rstIdVec.end());
         return uniqueIds.size() != rstIdVec.size();
      }

      bool hasInvalidVersion() const
      {
         return !majorVersion;
      }

      std::pair<bool, std::string> validateWithDetails() const
      {
         std::string invalidReasons;

         if (hasInvalidVersion())
            invalidReasons += "Has invalid version. ";
         if (hasInvalidIds())
            invalidReasons += "Contains invalid RSTId 0. ";
         if (hasDuplicateIds())
            invalidReasons += "Contains duplicate RSTIds. ";

         return std::make_pair(invalidReasons.empty(), invalidReasons);
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->majorVersion
            % obj->minorVersion
            % obj->coolDownPeriod
            % obj->reserved
            % obj->filePolicies
            % obj->rstIdVec;
      }

      std::vector<uint32_t> getRstIdVector() const
      {
         return rstIdVec;
      }

      uint16_t getCoolDownPeriod() const
      {
         return coolDownPeriod;
      }

      uint16_t getFilePolicies() const
      {
         return filePolicies;
      }

      void setRstIds(std::vector<uint32_t> ids)
      {
         this->rstIdVec = ids;
      }

      std::string toStr() const
      {
         std::stringstream ss;
         ss << "majorVersion: "   << std::to_string(majorVersion)   << "; "
            << "minorVersion: "   << std::to_string(minorVersion)   << "; "
            << "coolDownPeriod: " << std::to_string(coolDownPeriod) << "; "
            << "reserved: "       << std::to_string(reserved)       << "; "
            << "filePolicies: "   << std::to_string(filePolicies)   << "; "
            << "rstIds: [";

         bool first = true;
         for (auto id : rstIdVec)
         {
            if (!first)
               ss << ",";
            ss << std::to_string(id);
            first = false;
         }
         ss << "]";
         return ss.str();
      }

   private:
      uint8_t majorVersion = 0;
      uint8_t minorVersion = 0;
      uint16_t coolDownPeriod = 0;
      uint16_t reserved = 0;
      uint16_t filePolicies = 0;
      std::vector<uint32_t> rstIdVec;

      // constants (for default values)
      static const uint8_t DEFAULT_MAJOR_VER = 1;
      static const uint8_t DEFAULT_MINOR_VER = 0;
      static const uint16_t DEFAULT_COOLDOWN_PERIOD = 120;
      static const uint16_t DEFAULT_FILE_POLICY = 0x0001;
};


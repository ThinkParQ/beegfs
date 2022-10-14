#ifndef ENTRYINFOWITHDEPTH_H_
#define ENTRYINFOWITHDEPTH_H_

#include <common/storage/EntryInfo.h>

/**
 */
class EntryInfoWithDepth : public EntryInfo
{
   public:
      EntryInfoWithDepth() : EntryInfo(), entryDepth(0)
      {
      }

      EntryInfoWithDepth(const NumNodeID ownerNodeID, const std::string& parentEntryID,
         const std::string& entryID, const std::string& fileName, const DirEntryType entryType,
         const int featureFlags, const unsigned entryDepth) :
         EntryInfo(ownerNodeID, parentEntryID, entryID, fileName, entryType, featureFlags),
         entryDepth(entryDepth)
      {
      }

      EntryInfoWithDepth(const EntryInfo& entryInfo) : EntryInfo(entryInfo),
         entryDepth(0)
      {
      }

      virtual ~EntryInfoWithDepth()
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::base<EntryInfo>(obj)
            % obj->entryDepth;
      }

   private:
      uint32_t entryDepth; // 0-based path depth (incl. root dir)

   public:
      // inliners
      void set(const NumNodeID ownerNodeID, const std::string& parentEntryID,
         const std::string& entryID, const std::string& fileName, const DirEntryType entryType,
         const int featureFlags, const unsigned entryDepth)
      {
         EntryInfo::set(ownerNodeID, parentEntryID, entryID, fileName, entryType, featureFlags);

         this->entryDepth    = entryDepth;
      }

      unsigned getEntryDepth() const
      {
         return entryDepth;
      }

      void setEntryDepth(const unsigned entryDepth)
      {
         this->entryDepth = entryDepth;
      }
};


#endif /* ENTRYINFOWITHDEPTH_H_ */

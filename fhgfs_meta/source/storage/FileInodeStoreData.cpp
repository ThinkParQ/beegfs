#include "FileInodeStoreData.h"

void FileInodeStoreData::getPathInfo(PathInfo* outPathInfo)
{
   const char* logContext = "FileInode getPathInfo";
   unsigned flags;

   FileInodeOrigFeature origFeature  = getOrigFeature();
   switch (origFeature)
   {
      case FileInodeOrigFeature_TRUE:
      {
         flags = PATHINFO_FEATURE_ORIG;
      } break;

      case FileInodeOrigFeature_FALSE:
      {
         flags = 0;
      } break;

      default:
      case FileInodeOrigFeature_UNSET:
      {
         flags = PATHINFO_FEATURE_ORIG_UNKNOWN;
         LogContext(logContext).logErr("Bug: Unknown PathInfo status.");
      } break;

   }

   unsigned origParentUID = getOrigUID();
   const std::string& origParentEntryID = getOrigParentEntryID();

   outPathInfo->set(origParentUID, origParentEntryID, flags);
}

bool FileInodeStoreData::operator==(const FileInodeStoreData& second) const
{
   return inodeFeatureFlags == second.inodeFeatureFlags
      && inodeStatData == second.inodeStatData
      && entryID == second.entryID
      && stripePattern->stripePatternEquals(second.stripePattern)
      && origFeature == second.origFeature
      && origParentUID == second.origParentUID
      && origParentEntryID == second.origParentEntryID;
}

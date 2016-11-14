#ifndef TOOLKIT_MANAGEMENT_FILEENTRY_H_
#define TOOLKIT_MANAGEMENT_FILEENTRY_H_


#include <common/storage/StatData.h>



struct FileEntry
{
   std::string name;
   StatData statData;
};

typedef std::list<FileEntry> FileEntryList;
typedef FileEntryList::iterator FileEntryListIter;
typedef FileEntryList::const_iterator FileEntryListConstIter;


class FileEntrySort
{
   public:
      FileEntrySort(std::string sortBy)
      {
         this->sortBy = sortBy;
      }

      bool operator()(const FileEntry &x, const FileEntry &y)
      {
         // first of all check if one of the args is a dir => dirs always first
         if(S_ISDIR(x.statData.getMode()) && !S_ISDIR(y.statData.getMode() ) )
            return true;
         else
         if(!S_ISDIR(x.statData.getMode()) && S_ISDIR(x.statData.getMode() ) )
            return false;

         if(sortBy == "filesize")
            return x.statData.getFileSize() < y.statData.getFileSize();
         else
         if(sortBy == "mode")
            return x.statData.getMode() < y.statData.getMode();
         else
         if(sortBy == "ctime")
            return x.statData.getCreationTimeSecs() < y.statData.getCreationTimeSecs();
         else
         if(sortBy == "atime")
            return x.statData.getLastAccessTimeSecs() < y.statData.getLastAccessTimeSecs();
         else
         if(sortBy == "mtime")
            return x.statData.getModificationTimeSecs() < y.statData.getModificationTimeSecs();
         else
         if(sortBy == "userid")
            return x.statData.getUserID() < y.statData.getUserID();
         else
         if(sortBy == "groupid")
            return x.statData.getGroupID() < y.statData.getGroupID();
         else
            return x.name < y.name;
      }


   private:
      std::string sortBy;
};

#endif /* TOOLKIT_MANAGEMENT_FILEENTRY_H_ */

/*
 * Some methods from Meta's StorageTkEx defined here, so that they are also available for
 * fhgfs-ctl and other tools.
 *
 */

#ifndef METASTORAGETK_H_
#define METASTORAGETK_H_

#include <common/storage/Metadata.h>

class MetaStorageTk
{
   public:

      // inliners

      static std::string getMetaInodePath(const std::string inodePath, const std::string fileName)
      {
         return StorageTk::getHashPath(inodePath, fileName,
            META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);
      }

      /**
       * @param dirEntryID the entryID of the current directory
       */
      static std::string getMetaDirEntryPath(const std::string dentriesPath,
         const std::string dirEntryID)
      {
         return StorageTk::getHashPath(dentriesPath, dirEntryID,
            META_DENTRIES_LEVEL1_SUBDIR_NUM, META_DENTRIES_LEVEL2_SUBDIR_NUM);
      }

      /**
       * Returns the dirEntryID path.
       *
       * @param metaDirEntryPath - path to to the dirEntryName directory
       */
      static std::string getMetaDirEntryIDPath(const std::string metaDirEntryPath)
      {
         return metaDirEntryPath + "/" META_DIRENTRYID_SUB_STR "/";
      }
};


#endif /* METASTORAGETK_H_ */

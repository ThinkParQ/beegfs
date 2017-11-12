#ifndef METASTORAGETK_H_
#define METASTORAGETK_H_

#include <common/storage/Metadata.h>
#include <common/toolkit/StorageTk.h>


/*
 * Some methods from Meta's StorageTkEx defined here, so that they are also available for
 * fhgfs-ctl and other tools.
 */
class MetaStorageTk
{
   public:

      // inliners

      /**
       * Get complete path to a (non-inlined) inode.
       *
       * @param inodePath path to inodes subdir of general storage directory
       * @param fileName entryID of the inode
       */
      static std::string getMetaInodePath(const std::string inodePath, const std::string fileName)
      {
         return StorageTk::getHashPath(inodePath, fileName,
            META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);
      }

      /**
       * Get directory for a (non-inlined) inode.
       *
       * @param inodePath path to inodes subdir of general storage directory
       * @param fileName entryID of the inode
       */
      static std::pair<unsigned, unsigned> getMetaInodeHash(const std::string& fileName)
      {
         return StorageTk::getHash(fileName,
            META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);
      }

      /**
       * Get path to a contents directory (i.e. the dir containing the dentries by name).
       *
       * @param dirEntryID entryID of the directory for which the caller wants the contents path
       */
      static std::string getMetaDirEntryPath(const std::string dentriesPath,
         const std::string dirEntryID)
      {
         return StorageTk::getHashPath(dentriesPath, dirEntryID,
            META_DENTRIES_LEVEL1_SUBDIR_NUM, META_DENTRIES_LEVEL2_SUBDIR_NUM);
      }

      /**
       * Get path to the IDs subdir of a contents directory (i.e. the dir containing the dentries
       * by ID).
       *
       * @param metaDirEntryPath path to a contents dir (i.e. the dir containing dentries by name),
       * typically requires calling getMetaDirEntryPath() first
       */
      static std::string getMetaDirEntryIDPath(const std::string metaDirEntryPath)
      {
         return metaDirEntryPath + "/" META_DIRENTRYID_SUB_STR "/";
      }
};


#endif /* METASTORAGETK_H_ */

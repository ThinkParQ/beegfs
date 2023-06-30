/*
 * Dentry and inode serialization/deserialization.
 *
 * Note: Currently inodes and dentries are stored in exactly the same format, even if
 *       inodes are not inlined into a dentry.
 *       If we should add another inode-only format, all code linking inodes to dentries
 *       e.g. (MetaStore::unlinkInodeLaterUnlocked() calling dirInode->linkInodeToDir() must
 *       be updated.
 */

#include <program/Program.h>
#include <common/storage/StorageDefinitions.h>
#include "DiskMetaData.h"
#include "DirInode.h"
#include "FileInode.h"
#include "DirEntry.h"


#define DISKMETADATA_LOG_CONTEXT "DiskMetadata"


// 8-bit
#define DIRENTRY_STORAGE_FORMAT_VER2  2   // version beginning with release 2011.04
#define DIRENTRY_STORAGE_FORMAT_VER3  3   // version, same as V2, but removes the file name
                                          // from the dentry (for dir.dentries)
#define DIRENTRY_STORAGE_FORMAT_VER4  4   // version, which includes inlined inodes, deprecated
#define DIRENTRY_STORAGE_FORMAT_VER5  5   /* version, which includes inlined inodes
                                           * and chunk-path V3, StatData have a flags field */
#define DIRENTRY_STORAGE_FORMAT_VER6  6   /* VER5 + additional storage pool in pattern */

// 8-bit
#define DIRECTORY_STORAGE_FORMAT_VER1 1 // 16 bit node IDs
#define DIRECTORY_STORAGE_FORMAT_VER2 2 // 32 bit node IDs
#define DIRECTORY_STORAGE_FORMAT_VER3 3 // 32 bit node IDs + storage pool in pattern

void DiskMetaData::serializeFileInode(Serializer& ser)
{
   // inodeData set in constructor

   if (!DirEntryType_ISVALID(this->dentryDiskData->getDirEntryType() ) )
   {
      StatData* statData = this->inodeData->getInodeStatData();
      unsigned mode = statData->getMode();
      DirEntryType entryType = MetadataTk::posixFileTypeToDirEntryType(mode);
      this->dentryDiskData->setDirEntryType(entryType);

#ifdef BEEGFS_DEBUG
      const char* logContext = "Serialize FileInode";
      LogContext(logContext).logErr("Bug: entryType not set!");
      LogContext(logContext).logBacktrace();
#endif
   }

   /* We use this method to clone inodes which might be inlined into a dentry, so the real
    * meta type depends on if the inode is inlined or not.  */
   DiskMetaDataType metaDataType;
   if (this->dentryDiskData->dentryFeatureFlags & DENTRY_FEATURE_INODE_INLINE)
      metaDataType = DiskMetaDataType_FILEDENTRY;
   else
      metaDataType = DiskMetaDataType_FILEINODE;

   serializeInDentryFormat(ser, metaDataType);
}

void DiskMetaData::serializeDentry(Serializer& ser)
{
   DiskMetaDataType metaDataType;

   if (DirEntryType_ISDIR(this->dentryDiskData->getDirEntryType() ) )
      metaDataType = DiskMetaDataType_DIRDENTRY;
   else
      metaDataType = DiskMetaDataType_FILEDENTRY;

   serializeInDentryFormat(ser, metaDataType);
}

/*
 * Note: Current object state is used for the serialization
 */
void DiskMetaData::serializeInDentryFormat(Serializer& ser, DiskMetaDataType metaDataType)
{
   // note: the total amount of serialized data may not be larger than DIRENTRY_SERBUF_SIZE
   int dentryFormatVersion;

   // set the type into the entry (1 byte)
   ser % uint8_t(metaDataType);

   // storage-format-version (1 byte)
   if (DirEntryType_ISDIR(this->dentryDiskData->getDirEntryType()))
   {
      // metadata format version-3 for directories
      ser % uint8_t(DIRENTRY_STORAGE_FORMAT_VER3);

      dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER3;
   }
   else
   {
      if (metaDataType == DiskMetaDataType_FILEINODE)
      {
         dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER6;
      }
      else if ((!(this->dentryDiskData->getDentryFeatureFlags() &
         DENTRY_FEATURE_INODE_INLINE)))
      {
         dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER3;
      }
      else if (this->inodeData->getOrigFeature() == FileInodeOrigFeature_TRUE)
         dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER6;
      else
         dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER4;

      // metadata format version-4 for files (inlined inodes)
      ser % uint8_t(dentryFormatVersion);
   }

   // dentry feature flags (2 bytes)
   // note: for newly written/serialized dentries we always use the long nodeIDs
   this->dentryDiskData->addDentryFeatureFlag(DENTRY_FEATURE_32BITIDS);

   ser % uint16_t(this->dentryDiskData->getDentryFeatureFlags());

   // entryType (1 byte)
   // (note: we have a fixed position for the entryType byte: DIRENTRY_TYPE_BUF_POS)
   ser % uint8_t(this->dentryDiskData->getDirEntryType());

   // 3 bytes padding for 8 byte alignment
   ser.skip(3);

   // end of 8 byte header

   switch (dentryFormatVersion)
   {
      case DIRENTRY_STORAGE_FORMAT_VER3:
         serializeDentryV3(ser); // V3, currently for dirs only
         break;

      case DIRENTRY_STORAGE_FORMAT_VER4:
         serializeDentryV4(ser); // V4 for files with inlined inodes
         break;

      case DIRENTRY_STORAGE_FORMAT_VER5: // gets automatically upgraded to v6
         dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER6; // inlined inodes + chunk-path-V3
         inodeData->getStripePattern()->setStoragePoolId(StoragePoolStore::DEFAULT_POOL_ID);

         BEEGFS_FALLTHROUGH;

      case DIRENTRY_STORAGE_FORMAT_VER6:
         serializeDentryV6(ser); // inlined inodes + chunk-path-V3 + storage pools in pattern
         break;
   }
}

/*
 * Deserialize a dentry buffer. Here we only deserialize basic values and will continue with
 * version specific dentry sub functions.
 *
 * Note: Applies deserialized data directly to the current object
 */
void DiskMetaData::deserializeDentry(Deserializer& des)
{
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization)";
   // note: assumes that the total amount of serialized data may not be larger than
      // DIRENTRY_SERBUF_SIZE

   uint8_t formatVersion; // which dentry format version

   {
      uint8_t metaDataType;
      des % metaDataType;
   }

   des % formatVersion;

   { // dentry feature flags
      uint16_t dentryFeatureFlags;

      des % dentryFeatureFlags;
      if (!des.good())
      {
         std::string serialType = "Feature flags";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return;
      }

      bool compatCheckRes = checkFeatureFlagsCompat(
         dentryFeatureFlags, getSupportedDentryFeatureFlags() );
      if(unlikely(!compatCheckRes) )
      {
         des.setBad();
         LOG(GENERAL, ERR, "Incompatible DirEntry feature flags found.", hex(dentryFeatureFlags),
               hex(getSupportedDentryFeatureFlags()));

         return;
      }

      this->dentryDiskData->setDentryFeatureFlags(dentryFeatureFlags);
   }

   {
      // (note: we have a fixed position for the entryType byte: DIRENTRY_TYPE_BUF_POS)
      uint8_t type;

      des % type;
      this->dentryDiskData->setDirEntryType((DirEntryType)type );
   }

   // mirrorNodeID (depends on feature flag) + padding
   if(this->dentryDiskData->getDentryFeatureFlags() & DENTRY_FEATURE_MIRRORED)
   { // mirrorNodeID + padding
      // Note: we have an old-style mirrored file here; what we do is just throw away the mirror
      // information here, because we don't need it; when the file gets written back to disk it
      // will be written as unmirrored file!

      // first of all strip the feature flag, so we do not write it to disk again
      this->dentryDiskData->removeDentryFeatureFlag(DENTRY_FEATURE_MIRRORED);

      uint16_t mirrorNodeID; // will be thrown away

      des % mirrorNodeID;

      // 1 byte padding for 8 byte aligned header
      des.skip(1);
   }
   else
   { // 3 bytes padding for 8 byte aligned header
      des.skip(3);
   }

   // end of 8-byte header

   switch (formatVersion)
   {
      case DIRENTRY_STORAGE_FORMAT_VER3:
         deserializeDentryV3(des);
         return;

      case DIRENTRY_STORAGE_FORMAT_VER4:
      {
         // data inlined, so this node must be the owner
         App* app = Program::getApp();
         MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

         deserializeDentryV4(des);
         if (!des.good())
            return;

         // setting the owner node ID is a manual action, as it is not saved on disk
         // depending on whether the file is mirrored or not, we set nodeID oder buddyGroupID here
         NumNodeID ownerNodeID = this->inodeData->getIsBuddyMirrored() ?
            NumNodeID(metaBuddyGroupMapper->getLocalGroupID() ) : app->getLocalNode().getNumID();

         this->dentryDiskData->setOwnerNodeID(ownerNodeID);

         this->inodeData->setOrigFeature(FileInodeOrigFeature_FALSE); // V4 does not have it
      } break;

      case DIRENTRY_STORAGE_FORMAT_VER5:
      {
         // data inlined, so this node must be the owner
         App* app = Program::getApp();
         MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

         deserializeDentryV5(des);
         if (!des.good())
            return;

         // setting the owner node ID is a manual action, as it is not saved on disk
         // depending on whether the file is mirrored or not, we set nodeID oder buddyGroupID here
         NumNodeID ownerNodeID = this->inodeData->getIsBuddyMirrored() ?
            NumNodeID(metaBuddyGroupMapper->getLocalGroupID() ) : app->getLocalNode().getNumID();

         this->dentryDiskData->setOwnerNodeID(ownerNodeID);

         this->inodeData->setOrigFeature(FileInodeOrigFeature_TRUE); // V5 has the origFeature

         // for upgrade to V6 format, immediately add pool ID
         inodeData->getStripePattern()->setStoragePoolId(StoragePoolStore::DEFAULT_POOL_ID);
      } break;

      case DIRENTRY_STORAGE_FORMAT_VER6:
      {
         // data inlined, so this node must be the owner
         App* app = Program::getApp();
         MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

         deserializeDentryV6(des);
         if (!des.good())
            return;

         // setting the owner node ID is a manual action, as it is not saved on disk
         // depending on whether the file is mirrored or not, we set nodeID oder buddyGroupID here
         NumNodeID ownerNodeID = this->inodeData->getIsBuddyMirrored() ?
            NumNodeID(metaBuddyGroupMapper->getLocalGroupID() ) : app->getLocalNode().getNumID();

         this->dentryDiskData->setOwnerNodeID(ownerNodeID);

         this->inodeData->setOrigFeature(FileInodeOrigFeature_TRUE); // V5 has the origFeature
      } break;

      default:
         LogContext(logContext).logErr("Invalid Storage Format: " +
            StringTk::uintToStr((unsigned) formatVersion) );
         des.setBad();
   }
}

/*
 * Version 3 format, now only used for directories and for example for disposal files
 */
void DiskMetaData::serializeDentryV3(Serializer& ser)
{
   ser
      % serdes::stringAlign4(this->dentryDiskData->getEntryID())
      % this->dentryDiskData->getOwnerNodeID();
}

/**
 * Deserialize dentries, which have the V3 format.
 */
void DiskMetaData::deserializeDentryV3(Deserializer& des)
{
   {
      std::string entryID;

      des % serdes::stringAlign4(entryID);
      this->dentryDiskData->setEntryID(entryID);
      this->inodeData->setEntryID(entryID);
   }

   if (this->dentryDiskData->getDentryFeatureFlags() & DENTRY_FEATURE_32BITIDS)
   {
      // dentry uses 32 bit nodeIDs, so we can just use our regular NumNodeIDs
      NumNodeID ownerNodeID;

      des % ownerNodeID;
      this->dentryDiskData->setOwnerNodeID(ownerNodeID);
   }
   else
   {
      // dentry uses old-style 16 bit nodeIDs
      uint16_t ownerNodeID;

      des % ownerNodeID;
      this->dentryDiskData->setOwnerNodeID(NumNodeID(ownerNodeID));
   }
}

/*
 * Version 4 format, for files with inlined inodes
 */
void DiskMetaData::serializeDentryV4(Serializer& ser)
{
   StatData* statData           = this->inodeData->getInodeStatData();
   StripePattern* stripePattern = this->inodeData->getPattern();

   ser % inodeData->getInodeFeatureFlags();
   ser.skip(4); // unused, was the chunkHash
   ser
      % statData->serializeAs(StatDataFormat_DENTRYV4)
      % serdes::stringAlign4(this->dentryDiskData->getEntryID())
      % stripePattern;

   if (inodeData->getInodeFeatureFlags() & FILEINODE_FEATURE_HAS_VERSIONS)
      ser % inodeData->getVersion();
}

/**
 * Deserialize dentries, which have the V4 format, which includes inlined inodes and have the old
 * chunk path (V1, by directly in hash dirs)
 */
void DiskMetaData::deserializeDentryV4(Deserializer& des)
{
   uint32_t inodeFeatureFlags;

   {
      des % inodeFeatureFlags;
      if (!des.good())
         return;

      bool compatCheckRes = checkFeatureFlagsCompat(
         inodeFeatureFlags, getSupportedDentryV4FileInodeFeatureFlags() );
      if(unlikely(!compatCheckRes) )
      {
         des.setBad();
         LOG(GENERAL, ERR, "Incompatible FileInode feature flags found.", hex(inodeFeatureFlags),
               hex(getSupportedDentryV4FileInodeFeatureFlags()));

         return;
      }

      this->inodeData->setInodeFeatureFlags(inodeFeatureFlags);
   }

   // unused, was the chunkHash
   des.skip(4);

   des % this->inodeData->getInodeStatData()->serializeAs(StatDataFormat_DENTRYV4);

   // note: up to here only fixed integers length, below follow variable string lengths

   {
      std::string entryID;

      des % serdes::stringAlign4(entryID);
      this->dentryDiskData->setEntryID(entryID);
      this->inodeData->setEntryID(entryID);
   }

   // mirrorNodeID (depends on feature flag)
   if(inodeFeatureFlags & FILEINODE_FEATURE_MIRRORED)
   {
      // Note: we have an old-style mirrored file here; what we do is just throw away the mirror
      // information here, because we don't need it; when the file gets written back to disk it
      // will be written as unmirrored file!

      // first of all strip the feature flag, so we do not write it to disk again
      this->inodeData->removeInodeFeatureFlag(FILEINODE_FEATURE_MIRRORED);
      uint16_t mirrorNodeID; // will be thrown away

      des % mirrorNodeID;
   }

   {
      StripePattern* pattern = StripePattern::deserialize(des, false);
      this->inodeData->setPattern(pattern);
   }

   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_VERSIONS)
      des % inodeData->version;

   inodeData->addInodeFeatureFlag(FILEINODE_FEATURE_HAS_VERSIONS);

   // sanity checks

#ifdef BEEGFS_DEBUG
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization V4)";
   if (unlikely(!(this->dentryDiskData->getDentryFeatureFlags() & DENTRY_FEATURE_IS_FILEINODE)))
   {
      LogContext(logContext).logErr("Bug: inode data successfully deserialized, but "
         "the file-inode flag is not set. ");
      return;
   }
#endif
}

/*
 * Version 6 format, for files with inlined inodes and orig-parentID + orig-UID + storage pool
 *
 * Note: Serialization in V5 is not supported any longer => auto upgrade to v6
 */
void DiskMetaData::serializeDentryV6(Serializer& ser)
{
   StatData* statData           = this->inodeData->getInodeStatData();
   StripePattern* stripePattern = this->inodeData->getPattern();
   uint32_t inodeFeatureFlags   = inodeData->getInodeFeatureFlags();

   ser % inodeFeatureFlags;
   ser.skip(4); // unused (4 bytes) for 8 byte alignment
   ser % statData->serializeAs(StatDataFormat_FILEINODE);

   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_UID)
      ser % this->inodeData->getOrigUID();

   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_PARENTID)
      ser % serdes::stringAlign4(this->inodeData->getOrigParentEntryID());

   ser
      % serdes::stringAlign4(this->dentryDiskData->getEntryID())
      % stripePattern;

   if (inodeData->getInodeFeatureFlags() & FILEINODE_FEATURE_HAS_VERSIONS)
      ser % inodeData->getVersion();
}

/**
 * Deserialize dentries, which have the V5 or V6 format. Both include inlined inodes and have the
 * new chunk path (V2, which has UID and parentID); Additionally, V6 has a storage pool in stripe
 * pattern
 */
void DiskMetaData::deserializeDentryV5V6(Deserializer& des, bool hasStoragePool)
{
   unsigned inodeFeatureFlags;

   {
      des % inodeFeatureFlags;
      if (!des.good())
         return;

      bool compatCheckRes = checkFeatureFlagsCompat(
         inodeFeatureFlags, getSupportedDentryV5FileInodeFeatureFlags() );
      if(unlikely(!compatCheckRes) )
      {
         des.setBad();
         LOG(GENERAL, ERR, "Incompatible FileInode feature flags found.", hex(inodeFeatureFlags),
               hex(getSupportedDentryV5FileInodeFeatureFlags()));

         return;
      }

      this->inodeData->setInodeFeatureFlags(inodeFeatureFlags);
   }

   // unused, for alignment
   des.skip(4);

   StatData* statData = this->inodeData->getInodeStatData();

   des % statData->serializeAs(StatDataFormat_FILEINODE);

   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_UID)
   {
      unsigned origParentUID;

      des % origParentUID;
      this->inodeData->setOrigUID(origParentUID);
   }
   else
   {  // no separate field, orig-UID and UID are identical
      unsigned origParentUID = statData->getUserID();
      this->inodeData->setOrigUID(origParentUID);
   }

   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_ORIG_PARENTID)
   {
      std::string origParentEntryID;

      des % serdes::stringAlign4(origParentEntryID);
      this->inodeData->setDiskOrigParentEntryID(origParentEntryID);
   }

   // note: up to here only fixed integers length, below follow variable string lengths

   {
      std::string entryID;

      des % serdes::stringAlign4(entryID);
      this->dentryDiskData->setEntryID(entryID);
      this->inodeData->setEntryID(entryID);
   }

   if(inodeFeatureFlags & FILEINODE_FEATURE_MIRRORED)
   {
      // Note: we have an old-style mirrored file here; what we do is just throw away the mirror
      // information here, because we don't need it; when the file gets written back to disk it
      // will be written as unmirrored file!

      // first of all strip the feature flag, so we do not write it to disk again
      this->inodeData->removeInodeFeatureFlag(FILEINODE_FEATURE_MIRRORED);
      uint16_t mirrorNodeID; // will be thrown away

      des % mirrorNodeID;
   }

   {
      StripePattern* pattern = StripePattern::deserialize(des, hasStoragePool);
      this->inodeData->setPattern(pattern);
   }

   if (inodeFeatureFlags & FILEINODE_FEATURE_HAS_VERSIONS)
      des % inodeData->version;

   inodeData->addInodeFeatureFlag(FILEINODE_FEATURE_HAS_VERSIONS);
}

void DiskMetaData::deserializeDentryV5(Deserializer& des)
{
   // sanity checks
   deserializeDentryV5V6(des, false);

#ifdef BEEGFS_DEBUG
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization V5)";
   if (unlikely(!(this->dentryDiskData->getDentryFeatureFlags() & DENTRY_FEATURE_IS_FILEINODE)))
   {
      LogContext(logContext).logErr("Bug: inode data successfully deserialized, but "
         "the file-inode flag is not set. ");
      return;
   }
#endif
}

void DiskMetaData::deserializeDentryV6(Deserializer& des)
{
   // sanity checks
   deserializeDentryV5V6(des, true);

#ifdef BEEGFS_DEBUG
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization V6)";
   if (unlikely(!(this->dentryDiskData->getDentryFeatureFlags() & DENTRY_FEATURE_IS_FILEINODE)))
   {
      LogContext(logContext).logErr("Bug: inode data successfully deserialized, but "
         "the file-inode flag is not set. ");
      return;
   }
#endif
}

/**
 * This method is for file-inodes located in the inode-hash directories.
 */
void DiskMetaData::deserializeFileInode(Deserializer& des)
{
   // right now file inodes are stored in the dentry format only, that will probably change
   // later on.
   return deserializeDentry(des);
}


template<typename Inode, typename Ctx>
void DiskMetaData::serializeDirInodeCommonData(Inode& inode, Ctx& ctx)
{
   if (likely(inode.featureFlags & DIRINODE_FEATURE_EARLY_SUBDIRS))
      ctx % inode.numSubdirs;

   ctx % inode.statData.serializeAs(
         inode.featureFlags & DIRINODE_FEATURE_STATFLAGS
            ? StatDataFormat_DIRINODE
            : StatDataFormat_DIRINODE_NOFLAGS);

   if (unlikely(!(inode.featureFlags & DIRINODE_FEATURE_EARLY_SUBDIRS)))
      ctx % inode.numSubdirs;

   ctx
      % inode.numFiles
      % serdes::stringAlign4(inode.id)
      % serdes::stringAlign4(inode.parentDirID);
}

/*
 * Note: Current object state is used for the serialization
 */
void DiskMetaData::serializeDirInode(Serializer& ser, DirInode& inode)
{
   // note: the total amount of serialized data may not be larger than META_SERBUF_SIZE

   inode.featureFlags |= (DIRINODE_FEATURE_EARLY_SUBDIRS | DIRINODE_FEATURE_STATFLAGS);

   ser
      % uint8_t(DiskMetaDataType_DIRINODE)
      % uint8_t(DIRECTORY_STORAGE_FORMAT_VER3)
      % inode.featureFlags;

   serializeDirInodeCommonData<const DirInode>(inode, ser);

   ser
      % inode.ownerNodeID
      % inode.parentNodeID
      % inode.stripePattern;
}

/*
 * Deserialize a DirInode
 *
 * Note: Applies deserialized data directly to the current object
 *
 */
void DiskMetaData::deserializeDirInode(Deserializer& des, DirInode& outInode)
{
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (DirInode Deserialization)";
   // note: assumes that the total amount of serialized data may not be larger than META_SERBUF_SIZE

   uint8_t formatVersion;

   {
      uint8_t metaDataType;

      des % metaDataType;
      if (unlikely(des.good() && metaDataType != DiskMetaDataType_DIRINODE))
      {
         LogContext(logContext).logErr(
            std::string("Deserialization failed: expected DirInode, but got (numeric type): ")
            + StringTk::uintToStr((unsigned) metaDataType) );
         des.setBad();
         return;
      }
   }

   des % formatVersion;

   {
      des % outInode.featureFlags;
      if (!des.good())
         return;

      bool compatCheckRes = checkFeatureFlagsCompat(
         outInode.featureFlags, getSupportedDirInodeFeatureFlags() );
      if(unlikely(!compatCheckRes) )
      {
         LogContext(logContext).logErr("Incompatible DirInode feature flags found. "
            "Used flags (hex): " + StringTk::uintToHexStr(outInode.featureFlags) + "; "
            "Supported (hex): " + StringTk::uintToHexStr(getSupportedDirInodeFeatureFlags() ) );
         des.setBad();
         return;
      }
   }

   serializeDirInodeCommonData(outInode, des);

   bool hasStoragePool;

   switch (formatVersion)
   {
      case DIRECTORY_STORAGE_FORMAT_VER1:
      {
         uint16_t ownerNode;
         uint16_t parentNode;

         des
            % ownerNode
            % parentNode;

         outInode.ownerNodeID = NumNodeID(ownerNode);
         outInode.parentNodeID = NumNodeID(parentNode);
         hasStoragePool = false;
         break;
      }

      case DIRECTORY_STORAGE_FORMAT_VER2:
      {
         des
            % outInode.ownerNodeID
            % outInode.parentNodeID;

         hasStoragePool = false;
         break;
      }

      case DIRECTORY_STORAGE_FORMAT_VER3:
      {
         des
            % outInode.ownerNodeID
            % outInode.parentNodeID;

         hasStoragePool = true;
         break;
      }

      default:
      {
         LogContext(logContext).logErr("Incompatible DirInode version found. "
            "Version:" + StringTk::uintToStr(formatVersion));
         des.setBad();
         return;
      }
   }

   // mirrorNodeID (depends on feature flag)
   if(outInode.featureFlags & DIRINODE_FEATURE_MIRRORED)
   {
      // Note: we have an old-style mirrored file here; what we do is just throw away the mirror
      // information here, because we don't need it; when the file gets written back to disk it
      // will be written as unmirrored file!

      // first of all strip the feature flag, so we do not write it to disk again
      outInode.removeFeatureFlag(DIRINODE_FEATURE_MIRRORED);
      uint16_t mirrorNodeID; // will be thrown away

      des % mirrorNodeID;
   }

   outInode.stripePattern = StripePattern::deserialize(des, hasStoragePool);
   if (!hasStoragePool)
      outInode.getStripePattern()->setStoragePoolId(StoragePoolStore::DEFAULT_POOL_ID);
}

/**
 * @return mask of supported dentry feature flags
 */
unsigned DiskMetaData::getSupportedDentryFeatureFlags()
{
   return DENTRY_FEATURE_INODE_INLINE | DENTRY_FEATURE_IS_FILEINODE | DENTRY_FEATURE_MIRRORED
      | DENTRY_FEATURE_BUDDYMIRRORED | DENTRY_FEATURE_32BITIDS;
}


/**
 * @return mask of supported file inode feature flags, inlined into V4 Dentries
 */
unsigned DiskMetaData::getSupportedDentryV4FileInodeFeatureFlags()
{
   return FILEINODE_FEATURE_MIRRORED | FILEINODE_FEATURE_BUDDYMIRRORED |
      FILEINODE_FEATURE_HAS_VERSIONS;
}

/**
 * @return mask of supported file inode feature flags, inlined into V5 Dentries
 */
unsigned DiskMetaData::getSupportedDentryV5FileInodeFeatureFlags()
{
   return FILEINODE_FEATURE_MIRRORED | FILEINODE_FEATURE_BUDDYMIRRORED |
      FILEINODE_FEATURE_HAS_ORIG_PARENTID | FILEINODE_FEATURE_HAS_ORIG_UID |
      FILEINODE_FEATURE_HAS_VERSIONS;
}

/**
 * @return mask of supported dir inode feature flags
 */
unsigned DiskMetaData::getSupportedDirInodeFeatureFlags()
{
   return DIRINODE_FEATURE_EARLY_SUBDIRS | DIRINODE_FEATURE_MIRRORED | DIRINODE_FEATURE_STATFLAGS |
      DIRINODE_FEATURE_BUDDYMIRRORED;
}

/**
 * Compare usedFeatureFlags with supportedFeatureFlags to find out whether an unsupported
 * feature flag is used.
 *
 * @return false if an unsupported feature flag was set in usedFeatureFlags
 */
bool DiskMetaData::checkFeatureFlagsCompat(unsigned usedFeatureFlags,
   unsigned supportedFeatureFlags)
{
   unsigned unsupportedFlags = ~supportedFeatureFlags;

   return !(usedFeatureFlags & unsupportedFlags);
}

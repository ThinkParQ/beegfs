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

#define DISKMETADATA_LOG_CONTEXT "DISKMETADATA"

// 8-bit
#define DIRENTRY_STORAGE_FORMAT_VER2  2   // version beginning with release 2011.04
#define DIRENTRY_STORAGE_FORMAT_VER3  3   // version, same as V2, but removes the file name
                                          // from the dentry (for dir.dentries)
#define DIRENTRY_STORAGE_FORMAT_VER4  4   // version, which includes inlined inodes

#define DIRECTORY_STORAGE_FORMAT_VER   1

unsigned DiskMetaData::serializeFileInode(char* buf)
{
   // inodeDataPtr set in constructor

   StatData* statData = this->inodeDataPtr->getInodeStatData();
   unsigned mode = statData->getMode();
   this->entryType = MetadataTk::posixFileTypeToDirEntryType(mode);;

   return serializeInDentryFormat(buf, DiskMetaDataType_FILEINODE);
}

unsigned DiskMetaData::serializeDentry(char* buf)
{
   this->inodeDataPtr = this->getInodeData(); // point to our own object

   DiskMetaDataType metaDataType;

   if (DirEntryType_ISDIR(this->entryType) )
      metaDataType = DiskMetaDataType_DIRDENTRY;
   else
      metaDataType = DiskMetaDataType_FILEDENTRY;

   return serializeInDentryFormat(buf, metaDataType);

}

/*
 * Note: Current object state is used for the serialization
 */
unsigned DiskMetaData::serializeInDentryFormat(char* buf, DiskMetaDataType metaDataType)
{
   // note: the total amount of serialized data may not be larger than DIRENTRY_SERBUF_SIZE

   size_t bufPos = 0;
   int dentryFormatVersion;

   // set the type into the entry (1 byte)
   bufPos = Serialization::serializeUInt8(&buf[bufPos], metaDataType);

   // storage-format-version (1 byte)
   if (DirEntryType_ISDIR(this->entryType) )
   {
      // metadata format version-3 for directories
      bufPos += Serialization::serializeUInt8(&buf[bufPos], (uint8_t)DIRENTRY_STORAGE_FORMAT_VER3);

      dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER3;
   }
   else
   {
      // metadata format version-4 for files (inlined inodes)
      bufPos += Serialization::serializeUInt8(&buf[bufPos], (uint8_t) DIRENTRY_STORAGE_FORMAT_VER4);

      dentryFormatVersion = DIRENTRY_STORAGE_FORMAT_VER4;
   }

   // dentry feature flags (2 bytes)
   bufPos += Serialization::serializeUInt16(&buf[bufPos], (uint16_t)this->dentryFeatureFlags);

   // entryType (1 byte)
   // (note: we have a fixed position for the entryType byte: DIRENTRY_TYPE_BUF_POS)
   bufPos += Serialization::serializeUInt8(&buf[bufPos], (uint8_t)entryType);

   // 3 bytes padding for 8 byte alignment
   memset(&buf[bufPos], 0 , 3);
   bufPos += 3;

   // end of 8 byte header

   if (dentryFormatVersion == DIRENTRY_STORAGE_FORMAT_VER4)
      bufPos += this->serializeDentryV4(&buf[bufPos]); // V4 for files with inlined inodes
   else
      bufPos += this->serializeDentryV3(&buf[bufPos]); // V3 for directories

   return bufPos;
}

/*
 * Version 3 format, now only used for directories and for example for disposal files
 *
 * Note: Do not call directly, but call serializeDentry()
 */
unsigned DiskMetaData::serializeDentryV3(char* buf)
{
   size_t bufPos = 0;

   // ID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], this->updatedEntryID.size(),
      this->updatedEntryID.c_str() );

   // ownerNodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], this->ownerNodeID);


   return bufPos;
}

/*
 * Version 4 format, for files with inlined inodes
 *
 * Note: Do not call directly, but call serializeDentry()
 */
unsigned DiskMetaData::serializeDentryV4(char* buf)
{
   StatData* statData           = this->inodeDataPtr->getInodeStatData();
   StripePattern* stripePattern = this->inodeDataPtr->getPattern();

   size_t bufPos = 0;

   // inode feature flags (4 bytes)
   bufPos += Serialization::serializeUInt(&buf[bufPos], inodeDataPtr->getInodeFeatureFlags());

   // chunkHash
   bufPos += Serialization::serializeUInt(&buf[bufPos], inodeDataPtr->getChunkHash() );

   // statData (has 64-bit values, so buffer should be 8-bytes aligned)
   bufPos += statData->serialize(false, &buf[bufPos]); // right now only files, so always 'false'

   // ID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], this->entryID.size(),
      this->entryID.c_str() );

   // stripePattern
   bufPos += stripePattern->serialize(&buf[bufPos]);

   return bufPos;
}

/**
 * Return the type of an inode and deserialize into the correct structure. This is mostly useful
 * for fhgfs-fsck, which does not know which inode type to expect.
 *
 * FIXME Bernd / Christian: Needs to be tested. Probably with cppunit.
 */
DirEntryType DiskMetaData::deserializeInode(const char* buf, FileInode** outFileInode,
   DirInode** outDirInode)
{
   *outFileInode = NULL;
   *outDirInode  = NULL;

   FileInode *fileInode = NULL;
   DirInode * dirInode  = NULL;

   DiskMetaDataType metaDataType;
   DirEntryType entryType = DirEntryType_INVALID;


   { // DiskMetaDataType
      uint8_t type;
      unsigned metaDataTypeLen;

      size_t bufPos = DISKMETADATA_TYPE_BUF_POS;
      size_t bufLen = 1;

      if(!Serialization::deserializeUInt8(&buf[bufPos], bufLen-bufPos, &type,
         &metaDataTypeLen) )
         return DirEntryType_INVALID;

      metaDataType = (DiskMetaDataType) type;
      bufPos += metaDataTypeLen;
   }

   if (metaDataType == DiskMetaDataType_DIRDENTRY) // currently cannot have an inode included
      return DirEntryType_INVALID;

   if (metaDataType == DiskMetaDataType_DIRINODE)
   {
      entryType = DirEntryType_DIRECTORY;
      std::string entryID = ""; // unkown
      dirInode = new DirInode(entryID);

      bool deserialRes = deserializeDirInode(buf, dirInode);
      if (deserialRes == false)
      {
         delete dirInode;
         return DirEntryType_INVALID;
      }
   }
   else
   if (metaDataType == DiskMetaDataType_FILEINODE || DiskMetaDataType_FILEDENTRY)
   {
      entryType = DirEntryType_REGULARFILE;
      fileInode = new FileInode();

      bool deserializeRes = fileInode->deserializeMetaData(buf);
      if (deserializeRes == false)
      { // deserialization seems to have failed
         delete fileInode;
         return DirEntryType_INVALID;
      }
   }

   *outFileInode = fileInode;
   *outDirInode  = dirInode;
   return entryType;
}

/**
 * This method is for file-inodes located in the inode-hash directories.
 */
bool DiskMetaData::deserializeFileInode(const char* buf)
{
   const char* logContext = "DiskMetaData FileInode deserialization";

#if 0  // upgrade tool
   // right now file inodes are stored in the dentry format only, that will probably change
   // later on.
   return deserializeDentry(buf);
#endif

   size_t bufLen = META_SERBUF_SIZE;
   size_t bufPos = 0;

   { // metadata format version
      unsigned formatVersionFieldLen;
      unsigned formatVersionBuf; // we have only one format currently

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &formatVersionBuf,
         &formatVersionFieldLen) )
         return false;

      bufPos += formatVersionFieldLen;
   }

   { // feature flags (currently unused)
      bufPos += Serialization::serialLenUInt();
   }

   { // statData
      StatData* statData = this->inodeData.getInodeStatData();
      unsigned statLen;

      if (!statData->deserialize(false, &buf[bufPos], bufLen-bufPos, &statLen) )
         return false;

      bufPos += statLen;
   }

   { // ID
      unsigned idBufLen;
      unsigned idLen;
      const char* idStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &idStrStart, &idBufLen) )
         return false;

      this->entryID.assign(idStrStart, idLen);
      this->inodeData.setID(this->entryID);
      bufPos += idBufLen;
   }

   { // parentEntryID
      unsigned parentDirBufLen;
      unsigned parentDirLen;
      const char* parentDirStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &parentDirLen, &parentDirStrStart, &parentDirBufLen) )
         return false;

      // this->parentEntryID.assign(parentDirStrStart, parentDirLen);
      bufPos += parentDirBufLen;
   }

   { // parentNodeID
      unsigned parentNodeBufLen;
      const char* parentNodeID;
      unsigned parentNodeIDLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &parentNodeIDLen, &parentNodeID, &parentNodeBufLen) )
         return false;

      bufPos += parentNodeBufLen;
   }

   { // stripePattern
      unsigned patternBufLen;
      StripePatternHeader patternHeader;
      const char* patternStart;
      std::string serialType = "stripePattern";

      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &patternHeader, &patternStart, &patternBufLen) )
      {
         LogContext(logContext).logErr("Deserialization failed: " + serialType);
         return false;
      }

      StripePattern* pattern = StripePattern::createFromBuf(patternStart, patternHeader);
      if (unlikely(pattern->getPatternType() == STRIPEPATTERN_Invalid) )
      {
         LogContext(logContext).logErr("Deserialization failed: " + serialType);
         SAFE_DELETE(pattern);
         return false;
      }

      this->inodeData.setPattern(pattern);
      bufPos += patternBufLen;
   }

   return true;

}

/*
 * Deserialize a dentry buffer. Here we only deserialize basic values and will continue with
 * version specific dentry sub functions.
 *
 * Note: Applies deserialized data directly to the current object
 */
bool DiskMetaData::deserializeDentry(const char* buf)
{
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization)";
   // note: assumes that the total amount of serialized data may not be larger than
      // DIRENTRY_SERBUF_SIZE

   size_t bufLen = DIRENTRY_SERBUF_SIZE;
   size_t bufPos = 0;
   uint formatVersion; // which dentry format version

#if 0 // disabled for the upgrade tool
   { // DiskMetaDataType
      uint8_t metaDataType;
      unsigned metaDataTypeLen;

      if(!Serialization::deserializeUInt8(&buf[bufPos], bufLen-bufPos, &metaDataType,
         &metaDataTypeLen) )
      {
         std::string serialType = "DiskMetaDataType";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);
         return false;
      }

      bufPos += metaDataTypeLen;
   }
#endif

   { // metadata format version
      unsigned formatVersionFieldLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &formatVersion,
         &formatVersionFieldLen) )
      {
         std::string serialType = "Format version";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += formatVersionFieldLen;
   }

   { // feature flags
      unsigned featureFlagsFieldLen;
      unsigned dentryFeatureFlags;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &dentryFeatureFlags, &featureFlagsFieldLen) )
      {
         std::string serialType = "Format version";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      this->dentryFeatureFlags = dentryFeatureFlags; // for upgrade tool (type cast)
      this->inodeData.setDentryFeatureFlags(this->dentryFeatureFlags);
      bufPos += featureFlagsFieldLen;
   }

   { // entryType
     // (note: we have a fixed position for the entryType byte: DIRENTRY_TYPE_BUF_POS)
      unsigned typeLen;
      uint8_t type;

      if(!Serialization::deserializeUInt8(&buf[bufPos], bufLen-bufPos, &type, &typeLen) )
      {
         std::string serialType = "entryType";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      this->entryType = (DirEntryType)type;
      bufPos += typeLen;
   }

   // 7 bytes padding for 16 byte alignment (below the feature flag independent values)
   bufPos += 7;

   // end of 8-byte header

   switch (formatVersion)
   {
      case DIRENTRY_STORAGE_FORMAT_VER2:
      {
         unsigned dentry2Len;

         if (unlikely(!this->deserializeDentryV2(&buf[bufPos], bufLen - bufPos, &dentry2Len) ) )
            return false;

         bufPos += dentry2Len;
      } break;

#if 0 // disabled for upgrade tool
      case DIRENTRY_STORAGE_FORMAT_VER3:
      {
         unsigned dentry3Len;

         if (unlikely(!this->deserializeDentryV3(&buf[bufPos], bufLen - bufPos, &dentry3Len) ) )
            return false;

         bufPos += dentry3Len;
      } break;

      case DIRENTRY_STORAGE_FORMAT_VER4:
      {
         App* app = Program::getApp();
         this->ownerNodeID = app->getLocalNode()->getNumID(); // data inlined, so we ourselves
         unsigned dentry4Len;

         if (unlikely(!this->deserializeDentryV4(&buf[bufPos], bufLen - bufPos, &dentry4Len) ) )
            return false;

         bufPos += dentry4Len;
      } break;
#endif

      default:
      {
         LogContext(logContext).logErr("Invalid Storage Format: " +
            StringTk::uintToStr((unsigned) formatVersion) );
         return false;
      } break;

   }

   return true;
}

/**
 * Deserialize dentries, which have the V2 format.
 * NOTE: Do not call it directly, but only from DirEntry::deserializeMetaData()
 *
 */
bool DiskMetaData::deserializeDentryV2(const char* buf, size_t bufLen, unsigned* outLen)
{
   size_t bufPos = 0;

   { // name
      unsigned nameBufLen;
      unsigned nameLen;
      const char* nameStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &nameLen, &nameStrStart, &nameBufLen) )
         return false;

      // this->name.assign(nameStrStart, nameLen);
      bufPos += nameBufLen;
   }

   { // ID
      unsigned idBufLen;
      unsigned idLen;
      const char* idStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &idStrStart, &idBufLen) )
         return false;

      this->entryID.assign(idStrStart, idLen);
      bufPos += idBufLen;
   }

   { // ownerNodeID
      unsigned ownerBufLen;
      const char *ownerNodeID;
      unsigned idLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &ownerNodeID, &ownerBufLen) )
         return false;

      this->ownerNodeIDStr.assign(ownerNodeID, idLen);
      bufPos += ownerBufLen;
   }

   *outLen = bufPos;

   return true;
}


/**
 * Deserialize dentries, which have the V3 format.
 *
 * NOTE: Do not call it directly, but only from DiskMetaData::deserializeDentry()
 *
 */
bool DiskMetaData::deserializeDentryV3(const char* buf, size_t bufLen, unsigned* outLen)
{
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization V3)";
   size_t bufPos = 0;

   { // entryID
      unsigned idBufLen;
      unsigned idLen;
      const char* idStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &idStrStart, &idBufLen) )
      {
         std::string serialType = "entryID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      this->entryID.assign(idStrStart, idLen);
      bufPos += idBufLen;
   }

   { // ownerNodeID
      unsigned ownerBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &ownerNodeID, &ownerBufLen) )
      {
         std::string serialType = "ownerNodeID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += ownerBufLen;
   }

   *outLen = bufPos;

   return true;
}

/**
 * Deserialize dentries, which have the V4 format, which includes inlined inodes
 *
 * NOTE: Do not call it directly, but only from DiskMetaData::deserializeDentry()
 *
 */
bool DiskMetaData::deserializeDentryV4(const char* buf, size_t bufLen, unsigned* outLen)
{
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (Dentry Deserialization V4)";
   size_t bufPos = 0;

   { // inode feature flags
      unsigned inodeFeatureFlags;
      unsigned inodeFeatureFlagsLen;

      if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &inodeFeatureFlags,
         &inodeFeatureFlagsLen) )
      {
         std::string serialType = "inode feature flags";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      inodeData.setInodeFeatureFlags(inodeFeatureFlags);

      bufPos += inodeFeatureFlagsLen;
   }

   { // chunk hash
      unsigned chunkHash;
      unsigned chunkHashLen;

      if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &chunkHash,
         &chunkHashLen) )
      {
         std::string serialType = "chunkHash";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      inodeData.setChunkHash(chunkHash);

      bufPos += chunkHashLen;
   }

   { // statData
      StatData* statData = inodeData.getInodeStatData();
      unsigned statDataLen;

      if (!statData->deserialize(false, &buf[bufPos], bufLen-bufPos, &statDataLen) )
      {
         std::string serialType = "statData";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += statDataLen;
   }

   // note: up to here only fixed integers length, below follow variable string lengths

   { // ID
      unsigned idBufLen;
      unsigned idLen;
      const char* idStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &idStrStart, &idBufLen) )
      {
         std::string serialType = "entryID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      this->entryID.assign(idStrStart, idLen);
      this->inodeData.setID(this->entryID);
      bufPos += idBufLen;
   }


   { // stripePattern - not aligned, needs to be below aligned values
      unsigned patternBufLen;
      StripePatternHeader patternHeader;
      const char* patternStart;
      std::string serialType = "stripePattern";

      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &patternHeader, &patternStart, &patternBufLen) )
      {
         LogContext(logContext).logErr("Deserialization failed: " + serialType);
         return false;
      }

      StripePattern* pattern = StripePattern::createFromBuf(patternStart, patternHeader);
      if (unlikely(pattern->getPatternType() == STRIPEPATTERN_Invalid) )
      {
         LogContext(logContext).logErr("Deserialization failed: " + serialType);
         SAFE_DELETE(pattern);
         return false;
      }

      this->inodeData.setPattern(pattern);
      bufPos += patternBufLen;
   }

   // sanity checks

   #ifdef FHGFS_DEBUG
      if (unlikely(!(this->dentryFeatureFlags & DISKMETADATA_FEATURE_IS_FILEINODE) ) )
      {
         LogContext(logContext).logErr("Bug: inode data successfully deserialized, but "
            "the file-inode flag is not set. ");
         return false;
      }
   #endif

   *outLen = bufPos;

   return true;
}


/*
 * Note: Current object state is used for the serialization
 */
unsigned DiskMetaData::serializeDirInode(char* buf, DirInode* inode)
{
   // note: the total amount of serialized data may not be larger than META_SERBUF_SIZE

   size_t bufPos = 0;

   // DiskMetaDataType (1 byte)
   bufPos += Serialization::serializeUInt8(&buf[bufPos], DiskMetaDataType_DIRINODE);

   // metadata format version (1 byte); note: we have only one format currently
   bufPos += Serialization::serializeUInt8(&buf[bufPos], (uint8_t) DIRECTORY_STORAGE_FORMAT_VER);

   inode->featureFlags |= DIRINODE_FEATURE_EARLY_SUBDIRS;

   // feature flags (2 byte)
   bufPos += Serialization::serializeUInt16(&buf[bufPos], inode->featureFlags);

   // numSubdirs (4 byte), moved here with feature flag for 8 byte statData alignment
   bufPos += Serialization::serializeUInt(&buf[bufPos], inode->numSubdirs);

   // statData (starts with 8 byte, so we need to be 8 byte aligned here, ends with 4 bytes)
   bufPos += inode->statData.serialize(true, &buf[bufPos]);

   // numFiles
   bufPos += Serialization::serializeUInt(&buf[bufPos], inode->numFiles);

   // ID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      inode->updatedID.size(), inode->updatedID.c_str() );

   // parentDirID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      inode->updatedParentDirID.size(), inode->updatedParentDirID.c_str() );

   // ownerNodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], inode->ownerNodeID);

   // parentNodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], inode->parentNodeID);

   // stripePattern
   bufPos += inode->stripePattern->serialize(&buf[bufPos]);


   return bufPos;
}



/*
 * Deserialize a DirInode
 *
 * Note: Applies deserialized data directly to the current object
 *
 */
bool DiskMetaData::deserializeDirInode(const char* buf, DirInode* outInode)
{
   const char* logContext = DISKMETADATA_LOG_CONTEXT " (DirInode Deserialization)";
   // note: assumes that the total amount of serialized data may not be larger than META_SERBUF_SIZE

   size_t bufLen = META_SERBUF_SIZE;
   size_t bufPos = 0;

#if 0 // disabled for upgrade tool
   { // DiskMetaDataType
      uint8_t metaDataType;
      unsigned metaDataTypeLen;

      if(!Serialization::deserializeUInt8(&buf[bufPos], bufLen-bufPos, &metaDataType,
         &metaDataTypeLen) )
      {
         std::string serialType = "DiskMetaDataType";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      if (unlikely(metaDataType != DiskMetaDataType_DIRINODE))
      {
         LogContext(logContext).logErr(
            std::string("Deserialization failed: expected DirInode, but got (numeric type): ")
            + StringTk::uintToStr((unsigned) metaDataType) );

         return false;
      }

      bufPos += metaDataTypeLen;
   }
#endif

   { // metadata format version
      unsigned formatVersionFieldLen;
      unsigned formatVersionBuf; // we have only one format currently

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &formatVersionBuf,
         &formatVersionFieldLen) )
      {
         std::string serialType = "format version";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += formatVersionFieldLen;
   }

   { // feature flags (currently unused)
      unsigned featureFlagsLen;
      unsigned featureFlags;

      if ( !Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos,
         &featureFlags, &featureFlagsLen) )
      {
         std::string serialType = "feature flags";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      outInode->featureFlags = featureFlags;
      bufPos += featureFlagsLen;
   }

   { // statData
      unsigned statFieldLen;

      if (!outInode->statData.deserialize(true, &buf[bufPos], bufLen-bufPos, &statFieldLen) )
      {
         std::string serialType = "statData";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += statFieldLen;
   }

   { // numSubdirs
      unsigned subdirsFieldLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &(outInode->numSubdirs),
         &subdirsFieldLen) )
      {
         std::string serialType = "numSubDirs";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += subdirsFieldLen;
   }

   { // numFiles
      unsigned filesFieldLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &(outInode->numFiles),
         &filesFieldLen) )
      {
         std::string serialType = "numFiles";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      bufPos += filesFieldLen;
   }

   { // ID
      unsigned idBufLen;
      unsigned idLen;
      const char* idStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &idStrStart, &idBufLen) )
      {
         std::string serialType = "entryID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      outInode->id.assign(idStrStart, idLen);
      bufPos += idBufLen;
   }

   { // ownerNodeID
      unsigned ownerBufLen;
      unsigned ownerLen;
      const char* ownerStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &ownerLen, &ownerStrStart, &ownerBufLen) )
      {
         std::string serialType = "ownerNodeID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      outInode->ownerNodeIDStr.assign(ownerStrStart, ownerLen);
      bufPos += ownerBufLen;
   }

   { // parentDirID
      unsigned parentDirBufLen;
      unsigned parentDirLen;
      const char* parentDirStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &parentDirLen, &parentDirStrStart, &parentDirBufLen) )
      {
         std::string serialType = "parentDirID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      outInode->parentDirID.assign(parentDirStrStart, parentDirLen);
      bufPos += parentDirBufLen;
   }

   { // parentNodeID
      unsigned parentNodeBufLen;
      unsigned parentNodeLen;
      const char* parentNodeStrStart;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &parentNodeLen, &parentNodeStrStart, &parentNodeBufLen) )
      {
         std::string serialType = "parentNodeID";
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      outInode->parentNodeIDStr.assign(parentNodeStrStart, parentNodeLen);
      bufPos += parentNodeBufLen;
   }

   { // stripePattern
      unsigned patternBufLen;
      StripePatternHeader patternHeader;
      const char* patternStart;
      std::string serialType = "stripePattern";

      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &patternHeader, &patternStart, &patternBufLen) )
      {
         LogContext(logContext).logErr("Deserialization failed: " + serialType);

         return false;
      }

      outInode->stripePattern = StripePattern::createFromBuf(patternStart, patternHeader);
      if (unlikely(outInode->stripePattern->getPatternType() == STRIPEPATTERN_Invalid) )
      {
         LogContext(logContext).logErr("Deserialization failed: " + serialType);
         SAFE_DELETE(outInode->stripePattern);

         return false;
      }
      bufPos += patternBufLen;
   }


   return true;
}



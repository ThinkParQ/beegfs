/*
 * Information provided by stat()
 */

#include <common/toolkit/serialization/Serialization.h>

/**
 * serialize data for stat()
 *
 * @param isDiskDirInode  on disk data for directories differ from those of files, so if we going to
 *        to serialize data written to disk, we need to now if it is a directory or file.
 *        However, network serialization doe not differ between files and directories.
 */
size_t StatData::serialize(bool isDiskDirInode, char* outBuf)
{
   size_t bufPos = 0;

   // create time
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->creationTimeSecs);

   // atime
   bufPos += Serialization::serializeInt64(&outBuf[bufPos],
      this->settableFileAttribs.lastAccessTimeSecs);

   // mtime
   bufPos += Serialization::serializeInt64(&outBuf[bufPos],
      this->settableFileAttribs.modificationTimeSecs);

   // ctime
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->attribChangeTimeSecs);

   if (isDiskDirInode == false)
   {
      // fileSsize
      bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->fileSize);

      // nlink
      bufPos += Serialization::serializeUInt(&outBuf[bufPos], this->nlink);

      // contentsVersion
      bufPos += Serialization::serializeUInt(&outBuf[bufPos], this->contentsVersion);
   }

   // uid
   bufPos += Serialization::serializeUInt(&outBuf[bufPos], this->settableFileAttribs.userID);

   // gid
   bufPos += Serialization::serializeUInt(&outBuf[bufPos], this->settableFileAttribs.groupID);

   // mode
   bufPos += Serialization::serializeUInt(&outBuf[bufPos], this->settableFileAttribs.mode);

   return bufPos;
}

/**
 * Deserialize data for stat()
 *
 * @param isDiskDirInode  see StatData::serialize()
 */
bool StatData::deserialize(bool isDiskDirInode, const char* buf, size_t bufLen, unsigned* outLen)
{

   /* TODO: All those integers could be deserialized using a type conversion
    * (my_struct) (buf), which would simplify the code and also be faster.
    * The data just could not be used between architectures, or we would need to introduce
    * endian conversions */

   unsigned bufPos = 0;

   { // creationTime
      unsigned createTimeFieldLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
         &this->creationTimeSecs, &createTimeFieldLen) )
         return false;

      bufPos += createTimeFieldLen;
   }

   { // aTime
      unsigned aTimeFieldLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
         &this->settableFileAttribs.lastAccessTimeSecs, &aTimeFieldLen) )
         return false;

      bufPos += aTimeFieldLen;
   }

   { // mtime
      unsigned mTimeFieldLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
         &this->settableFileAttribs.modificationTimeSecs, &mTimeFieldLen) )
         return false;

      bufPos += mTimeFieldLen;
   }

   { // ctime
      unsigned cTimeFieldLen;
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
         &this->attribChangeTimeSecs, &cTimeFieldLen) )
         return false;

      bufPos += cTimeFieldLen;
   }

   if (isDiskDirInode == false)
   {
      { // fileSize
         unsigned sizeFieldLen;
         if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
            &this->fileSize, &sizeFieldLen) )
            return false;

         bufPos += sizeFieldLen;
      }

      { // nlink
         unsigned nlinkFieldLen;

         if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
            &this->nlink, &nlinkFieldLen) )
            return false;

         bufPos += nlinkFieldLen;
      }

      { // contentsVersion
         unsigned contentsFieldLen;

         if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
            &this->contentsVersion, &contentsFieldLen) )
            return false;

         bufPos += contentsFieldLen;
      }

   }
   { // uid
      unsigned uidFieldLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &this->settableFileAttribs.userID, &uidFieldLen) )
         return false;

      bufPos += uidFieldLen;
   }

   { // gid
      unsigned gidFieldLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &this->settableFileAttribs.groupID, &gidFieldLen) )
         return false;

      bufPos += gidFieldLen;
   }

   { // mode
      unsigned modeFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &this->settableFileAttribs.mode, &modeFieldLen) )
         return false;

      bufPos += modeFieldLen;
   }

   *outLen = bufPos;
   return true;
}

unsigned StatData::serialLen(void)
{
   unsigned length =
      Serialization::serialLenInt64()  + // creationTimeSec
      Serialization::serialLenInt64()  + // lastAccessTimeSecs
      Serialization::serialLenInt64()  + // modificationTimeSecs
      Serialization::serialLenInt64()  + // attribChangeTimeSecs
      Serialization::serialLenInt64()  + // fileSize
      Serialization::serialLenUInt()   + // nlink
      Serialization::serialLenUInt()   + // contentsVersion
      Serialization::serialLenUInt()   + // userID
      Serialization::serialLenUInt()   + // groupID
      Serialization::serialLenUInt();    // mode

   return length;
}

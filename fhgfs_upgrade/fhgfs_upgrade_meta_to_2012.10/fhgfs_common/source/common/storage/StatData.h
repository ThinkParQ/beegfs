/*
 * Information provided by stat()
 *
 * Remember to keep this class in sync with StatData.h of fhgfs_client_module!!!
 */

#ifndef STATDATA_H_
#define STATDATA_H_

#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/TimeAbs.h>


class StatData;


class StatData
{
   public:
      StatData() {}

      // used on file creation
      StatData(int mode, int64_t uid, int64_t gid)
      {
         int64_t nowSecs = TimeAbs().getTimeval()->tv_sec;

         this->fileSize             = 0;
         this->creationTimeSecs     = nowSecs;
         this->attribChangeTimeSecs = nowSecs;
         this->nlink                = 1;
         this->contentsVersion      = 0;

         this->settableFileAttribs.mode    = mode;
         this->settableFileAttribs.userID  = uid;
         this->settableFileAttribs.groupID = gid;
         this->settableFileAttribs.modificationTimeSecs = nowSecs;
         this->settableFileAttribs.lastAccessTimeSecs   = nowSecs;

      }

   private:

      int64_t fileSize;
      int64_t creationTimeSecs;     // real creation time
      int64_t attribChangeTimeSecs; // this corresponds to unix ctime
      unsigned nlink;

      SettableFileAttribs settableFileAttribs;

      unsigned contentsVersion; // inc'ed when file contents are modified (for cache invalidation)


   public:

      size_t serialize(bool isDiskDirInode, char* outBuf);
      bool deserialize(bool isDiskDirInode, const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen();

      // setters

      void set(int64_t fileSize, SettableFileAttribs* settableAttribs,
         int64_t createTime, int64_t attribChangeTime, unsigned nlink, unsigned contentsVersion)
      {
         this->fileSize             = fileSize;
         this->creationTimeSecs     = createTime;
         this->attribChangeTimeSecs = attribChangeTime;
         this->nlink                = nlink;

         this->settableFileAttribs = *settableAttribs;

         this->contentsVersion      = contentsVersion;
      }

      /**
       * Update statData with the new given data
       *
       * Note: We do not update everything (such as creationTimeSecs), but only values that
       *       really make sense to be updated at all.
       */
      void set(StatData* newStatData)
      {
         this->fileSize             = newStatData->fileSize;
         this->attribChangeTimeSecs = newStatData->attribChangeTimeSecs;
         this->nlink                = newStatData->nlink;

         this->settableFileAttribs  = newStatData->settableFileAttribs;

         this->contentsVersion      = newStatData->contentsVersion;
      }

      // getters
      void get(StatData& outStatData)
      {
         outStatData.fileSize             = this->fileSize;
         outStatData.creationTimeSecs     = this->creationTimeSecs;
         outStatData.attribChangeTimeSecs = this->attribChangeTimeSecs;
         outStatData.nlink                = this->nlink;

         outStatData.settableFileAttribs = this->settableFileAttribs;

         outStatData.contentsVersion      = this->contentsVersion;
      }

      void setAttribChangeTimeSecs(int64_t attribChangeTimeSecs)
      {
         this->attribChangeTimeSecs = attribChangeTimeSecs;
      }

      void setFileSize(int64_t fileSize)
      {
         this->fileSize = fileSize;
      }

      void setMode(int mode)
      {
         this->settableFileAttribs.mode = mode;
      }

      void setModificationTimeSecs(int64_t mtime)
      {
         this->settableFileAttribs.modificationTimeSecs = mtime;
      }

      void setLastAccessTimeSecs(int64_t atime)
      {
         this->settableFileAttribs.lastAccessTimeSecs = atime;
      }

      void setUserID(unsigned uid)
      {
         this->settableFileAttribs.userID = uid;
      }

      void setGroupID(unsigned gid)
      {
         this->settableFileAttribs.groupID = gid;
      }

      void setSettableFileAttribs(SettableFileAttribs newAttribs)
      {
         this->settableFileAttribs = newAttribs;
      }

      void setNumHardLinks(unsigned numHardLinks)
      {
         this->nlink = numHardLinks;
      }


      // getters
      int64_t getFileSize() const
      {
         return this->fileSize;
      }

      int getCreationTimeSecs() const
      {
         return this->creationTimeSecs;
      }

      int64_t getAttribChangeTimeSecs() const
      {
         return this->attribChangeTimeSecs;
      }

      int64_t getModificationTimeSecs() const
      {
         return this->settableFileAttribs.modificationTimeSecs;
      }

      int64_t getLastAccessTimeSecs() const
      {
         return this->settableFileAttribs.lastAccessTimeSecs;
      }

      unsigned getNumHardlinks() const
      {
         return this->nlink;
      }

      SettableFileAttribs* getSettableFileAttribs(void)
      {
         return &this->settableFileAttribs;
      }

      unsigned getMode() const
      {
         return this->settableFileAttribs.mode;
      }

      unsigned getUserID() const
      {
         return this->settableFileAttribs.userID;
      }

      unsigned getGroupID() const
      {
         return this->settableFileAttribs.groupID;
      }

};



#endif /* STATDATA_H_ */

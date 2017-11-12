/*
 * File creation information
 *
 */

#ifndef MKFILEDETAILS_H_
#define MKFILEDETAILS_H_

struct MkFileDetails
{
      MkFileDetails(const std::string& newName, const unsigned userID, const unsigned groupID,
            const int mode, const int umask, int64_t createTime) :
         newName(newName), userID(userID), groupID(groupID), mode(mode), umask(umask),
         createTime(createTime)
      { }

      void setNewEntryID(const char* newEntryID)
      {
         this->newEntryID = newEntryID;
      }

      std::string newName;
      std::string newEntryID; // only used for mirroring on secondary
      unsigned userID;
      unsigned groupID;
      int mode;
      int umask;
      int64_t createTime;
};


#endif /* MKFILEDETAILS_H_ */

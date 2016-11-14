/*
 * File creation information
 *
 */

#ifndef MKFILEDETAILS_H_
#define MKFILEDETAILS_H_

struct MkFileDetails
{
      MkFileDetails(std::string newName, unsigned userID, unsigned groupID, int mode) :
         newName(newName), userID(userID), groupID(groupID), mode(mode)
      {
         // see initializer list
      }

      std::string newName;
      unsigned userID;
      unsigned groupID;
      int mode;
};


#endif /* MKFILEDETAILS_H_ */

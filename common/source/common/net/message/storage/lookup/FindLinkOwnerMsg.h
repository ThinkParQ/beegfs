#ifndef FINDLINKOWNERMSG_H_
#define FINDLINKOWNERMSG_H_

#include <common/net/message/SimpleStringMsg.h>

/**
 * intended to be used to find the link owner of a given entry ID
 *
 * at the moment this is only a hint, because the link owner saved in entries
 * is not guaranteed to be correct.
 */
class FindLinkOwnerMsg : public SimpleStringMsg
{
   public:
      FindLinkOwnerMsg(std::string& entryID) :
         SimpleStringMsg(NETMSGTYPE_FindLinkOwner, entryID.c_str())
      {
      }

      FindLinkOwnerMsg() : SimpleStringMsg(NETMSGTYPE_FindLinkOwner)
      {
      }


   private:


   public:
      // getters & setters
      std::string getEntryID()
      {
         return std::string(getValue());
      }
};

#endif /* FINDLINKOWNERMSG_H_ */

#ifndef FINDLINKOWNERRESPMSG_H_
#define FINDLINKOWNERRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class FindLinkOwnerRespMsg : public NetMessage
{
   public:

      /**
       * @param result 1 if an error occurs, otherwise 0
       * @param linkownerNodeID the saved owner of the link
       */
      FindLinkOwnerRespMsg(int result, uint16_t linkOwnerNodeID, std::string &parentDirID) :
         NetMessage(NETMSGTYPE_FindLinkOwnerResp)
      {
         this->result = result;
         this->linkOwnerNodeID = linkOwnerNodeID;
         this->parentDirID = parentDirID.c_str();
         this->parentDirIDLen = strlen(this->parentDirID);
      }

      /**
       * For deserialization only
       */
      FindLinkOwnerRespMsg() : NetMessage(NETMSGTYPE_FindLinkOwnerResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() +
            Serialization::serialLenStr(parentDirIDLen) +
            Serialization::serialLenUShort(); // linkOwner
      }


   private:
      int result;
      uint16_t linkOwnerNodeID;
      unsigned parentDirIDLen;
      const char* parentDirID;

   public:

      // inliners

      // getters & setters
      int getResult()
      {
         return result;
      }

      uint16_t getLinkOwnerNodeID()
      {
         return linkOwnerNodeID;
      }

      std::string getParentDirID()
      {
         return parentDirID;
      }
};

#endif /*FINDLINKOWNERRESPMSG_H_*/

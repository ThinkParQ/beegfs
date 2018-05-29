#ifndef FINDLINKOWNERRESPMSG_H_
#define FINDLINKOWNERRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>

class FindLinkOwnerRespMsg : public NetMessageSerdes<FindLinkOwnerRespMsg>
{
   public:

      /**
       * @param result 1 if an error occurs, otherwise 0
       * @param linkownerNodeID the saved owner of the link
       */
      FindLinkOwnerRespMsg(int result, NumNodeID linkOwnerNodeID, std::string &parentDirID) :
         BaseType(NETMSGTYPE_FindLinkOwnerResp)
      {
         this->result = result;
         this->linkOwnerNodeID = linkOwnerNodeID;
         this->parentDirID = parentDirID.c_str();
         this->parentDirIDLen = strlen(this->parentDirID);
      }

      /**
       * For deserialization only
       */
      FindLinkOwnerRespMsg() : BaseType(NETMSGTYPE_FindLinkOwnerResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % serdes::rawString(obj->parentDirID, obj->parentDirIDLen)
            % obj->linkOwnerNodeID;
      }

   private:
      int32_t result;
      NumNodeID linkOwnerNodeID;
      unsigned parentDirIDLen;
      const char* parentDirID;

   public:

      // inliners

      // getters & setters
      int getResult()
      {
         return result;
      }

      NumNodeID getLinkOwnerNodeID()
      {
         return linkOwnerNodeID;
      }

      std::string getParentDirID()
      {
         return parentDirID;
      }
};

#endif /*FINDLINKOWNERRESPMSG_H_*/

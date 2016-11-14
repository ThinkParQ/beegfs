#ifndef MKDIRMSG_H_
#define MKDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MkDirMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param parentEntryInfo just a reference, so do not free it as long as you use this object!
       * @param preferredNodes just a reference, so do not free it as long as you use this object!
       */
      MkDirMsg(EntryInfo* parentEntryInfo, std::string& newDirName,
         unsigned userID, unsigned groupID, int mode,
         UInt16List* preferredNodes) : NetMessage(NETMSGTYPE_MkDir)
      {
         this->parentEntryInfoPtr = parentEntryInfo;
         this->newDirName = newDirName.c_str();
         this->newDirNameLen = newDirName.length();
         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->preferredNodes = preferredNodes;
      }

      /**
       * For deserialization only!
       */
      MkDirMsg() : NetMessage(NETMSGTYPE_MkDir) {}

      TestingEqualsRes testingEquals(NetMessage *msg);

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt() + // userID
            Serialization::serialLenUInt() + // groupID
            Serialization::serialLenInt() + // mode
            this->parentEntryInfoPtr->serialLen() +
            Serialization::serialLenStrAlign4(this->newDirNameLen) +
            Serialization::serialLenUInt16List(preferredNodes); // preferredNodes
      }


   private:
      
      unsigned userID;
      unsigned groupID;
      int mode;

      // serialization / deserialization
      const char* newDirName;
      unsigned newDirNameLen;

      // for serialization
      EntryInfo* parentEntryInfoPtr; // not owned by this object
      UInt16List* preferredNodes; // not owned by this object!

      // for deserialization
      EntryInfo parentEntryInfo;
      unsigned prefNodesElemNum;
      const char* prefNodesListStart;
      unsigned prefNodesBufLen;

      
   public:

      void parsePreferredNodes(UInt16List* outNodes)
      {
         Serialization::deserializeUInt16List(
            prefNodesBufLen, prefNodesElemNum, prefNodesListStart, outNodes);
      }

      // getters & setters
      unsigned getUserID()
      {
         return userID;
      }
      
      unsigned getGroupID()
      {
         return groupID;
      }
      
      int getMode()
      {
         return mode;
      }

      EntryInfo* getParentInfo(void)
      {
         return &this->parentEntryInfo;
      }

      const char* getNewDirName(void)
      {
         return this->newDirName;
      }

};

#endif /*MKDIRMSG_H_*/

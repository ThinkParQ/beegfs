#ifndef MKFILEMSG_H_
#define MKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MkFileMsg : public NetMessage
{
   public:
      
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkFileMsg(EntryInfo* parentInfo, std::string& newName,
         unsigned userID, unsigned groupID, int mode,
         UInt16List* preferredTargets) : NetMessage(NETMSGTYPE_MkFile)
      {
         this->parentInfoPtr = parentInfo;
         this->newName = newName.c_str();
         this->newNameLen = newName.length();
         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->preferredTargets = preferredTargets;
      }

      
   protected:
      /**
       * For deserialization only!
       */
      MkFileMsg() : NetMessage(NETMSGTYPE_MkFile) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt() + // userID
            Serialization::serialLenUInt() + // groupID
            Serialization::serialLenInt() + // mode
            this->parentInfoPtr->serialLen() + // entryInfo
            Serialization::serialLenStrAlign4(this->newNameLen) + // newName
            Serialization::serialLenUInt16List(preferredTargets); // preferredTargets
      }


   private:
      
      const char* newName;
      unsigned newNameLen;
      unsigned userID;
      unsigned groupID;
      int mode;
   
      // for serialization
      EntryInfo* parentInfoPtr;
      UInt16List* preferredTargets; // not owned by this object!

      // for deserialization
      EntryInfo parentInfo;
      unsigned prefTargetsElemNum;
      const char* prefTargetsListStart;
      unsigned prefTargetsBufLen;

      
   public:
   
      void parsePreferredNodes(UInt16List* outNodes)
      {
         Serialization::deserializeUInt16List(
            prefTargetsBufLen, prefTargetsElemNum, prefTargetsListStart, outNodes);
      }
      
      // getters & setters
      unsigned getUserID()
      {
         return this->userID;
      }
      
      unsigned getGroupID()
      {
         return this->groupID;
      }
      
      int getMode()
      {
         return this->mode;
      }

      const char* getNewName(void)
      {
         return this->newName;
      }

      EntryInfo* getParentInfo(void)
      {
         return &this->parentInfo;
      }
};

#endif /*MKFILEMSG_H_*/

#ifndef LOOKUPINTENTMSG_H_
#define LOOKUPINTENTMSG_H_

#include <common/net/message/NetMessage.h>


/* Keep these flags in sync with the client msg flags:
   fhgfs_client_mode/source/closed/net/message/storage/LookupIntentMsg.h */

#define LOOKUPINTENTMSG_FLAG_REVALIDATE         1 /* revalidate entry, cancel if invalid */
#define LOOKUPINTENTMSG_FLAG_CREATE             2 /* create file */
#define LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE    4 /* exclusive file creation */
#define LOOKUPINTENTMSG_FLAG_OPEN               8 /* open file */
#define LOOKUPINTENTMSG_FLAG_STAT              16 /* stat file */


class LookupIntentMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * This just prepares the basic lookup. Use the additional addIntent...() methods to do
       * more than just the lookup.
       *
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       * @param entryName just a reference, so do not free it as long as you use this object!
       */
      LookupIntentMsg(EntryInfo* parentInfo, std::string& entryName) :
         NetMessage(NETMSGTYPE_LookupIntent)
      {
         this->intentFlags = 0;

         this->parentInfoPtr = parentInfo;
         this->entryName = entryName;
      }

      /**
       * Used for revalidate intent
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       * @param entryInfo  just a reference, so do not free it as long as you use this object!
       *
       */
      LookupIntentMsg(EntryInfo* parentInfo, EntryInfo* entryInfo) :
         NetMessage(NETMSGTYPE_LookupIntent)
      {
         this->intentFlags = LOOKUPINTENTMSG_FLAG_REVALIDATE;

         this->parentInfoPtr = parentInfo;
         this->entryInfoPtr  = entryInfo;
      }

   protected:
      LookupIntentMsg() : NetMessage(NETMSGTYPE_LookupIntent)
      {
      }

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         unsigned msgLength = NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt()                      + // intentFlags
            this->parentInfoPtr->serialLen();                    // parentInfo


         if (!this->intentFlags & LOOKUPINTENTMSG_FLAG_REVALIDATE)
            Serialization::serialLenStrAlign4(entryName.length() );     // entryName

         if(this->intentFlags & LOOKUPINTENTMSG_FLAG_REVALIDATE)
         {
            msgLength += this->entryInfoPtr->serialLen();
         }

         if(this->intentFlags & LOOKUPINTENTMSG_FLAG_OPEN)
         {
            msgLength += Serialization::serialLenUInt()         + // accessFlags
               Serialization::serialLenStrAlign4(sessionIDLen);   // sessionID
         }

         if(this->intentFlags & LOOKUPINTENTMSG_FLAG_CREATE)
         {
            msgLength += Serialization::serialLenUInt()         + // userID
               Serialization::serialLenUInt()                   + // groupID
               Serialization::serialLenInt()                    + // mode
               Serialization::serialLenUInt16List(preferredTargets); // preferredTargets
         }

         return msgLength;
      }


   private:
      int intentFlags; // combination of LOOKUPINTENTMSG_FLAG_...

      // on revalidate retrieved from entryInfo in deserialization method
      std::string entryName; // (lookup data)

      unsigned userID; // (file creation data)
      unsigned groupID; // (file creation data)
      int mode; // file creation mode permission bits  (file creation data)

      unsigned sessionIDLen; // (file open data)
      const char* sessionID; // (file open data)
      unsigned accessFlags; // OPENFILE_ACCESS_... flags (file open data)

      // for serialization
      UInt16List* preferredTargets; // not owned by this object! (file creation data)
      EntryInfo* parentInfoPtr;
      EntryInfo* entryInfoPtr; // only set on revalidate

      // for deserialization
      EntryInfo parentInfo;
      EntryInfo entryInfo;               // (revalidate data)
      unsigned prefTargetsElemNum;       // (file creation data)
      const char* prefTargetsListStart;  // (file creation data)
      unsigned prefTargetsBufLen;        // (file creation data)



   public:

      void addIntentCreate(unsigned userID, unsigned groupID, int mode,
         UInt16List* preferredTargets)
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_CREATE;

         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->preferredTargets = preferredTargets;
      }

      void addIntentCreateExclusive()
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE;
      }

      /**
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      void addIntentOpen(const char* sessionID, unsigned accessFlags)
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_OPEN;

         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);

         this->accessFlags = accessFlags;
      }

      void addIntentStat()
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_STAT;
      }

      void parsePreferredTargets(UInt16List* outTargets)
      {
         Serialization::deserializeUInt16List(
            prefTargetsBufLen, prefTargetsElemNum, prefTargetsListStart, outTargets);
      }

      // getters & setters

      int getIntentFlags() const
      {
         return this->intentFlags;
      }

      std::string getEntryName()
      {
         return this->entryName;
      }


      EntryInfo* getParentInfo()
      {
         return &this->parentInfo;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      unsigned getUserID() const
      {
         return this->userID;
      }

      unsigned getGroupID() const
      {
         return this->groupID;
      }

      int getMode() const
      {
         return mode;
      }

      const char* getSessionID() const
      {
         return sessionID;
      }

      unsigned getAccessFlags() const
      {
         return accessFlags;
      }

};

#endif /* LOOKUPINTENTMSG_H_ */

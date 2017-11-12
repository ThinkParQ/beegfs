#include "LookupIntentMsg.h"

bool LookupIntentMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // intentFlags
      unsigned intentFlagsLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &intentFlags, &intentFlagsLen) )
         return false;

      bufPos += intentFlagsLen;
   }

   { // parentInfo
      unsigned parentLen;

      if(!this->parentInfo.deserialize(&buf[bufPos], bufLen-bufPos, &parentLen))
         return false;

      bufPos += parentLen;
   }

   if(!(this->intentFlags & LOOKUPINTENTMSG_FLAG_REVALIDATE))
   {
      { // entryName
         unsigned entryBufLen;
         const char* entryNameChar;
         unsigned entryNameLen;

         if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
            &entryNameLen, &entryNameChar, &entryBufLen) )
            return false;

         this->entryName.assign(entryNameChar, entryNameLen);
         bufPos += entryBufLen;
      }
   }
   else
   {
      { // entryInfo
         unsigned entryBufLen;

         if (!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
            return false;

         this->entryName = this->entryInfo.getFileName();
         bufPos += entryBufLen;
      }
   }

   if(this->intentFlags & LOOKUPINTENTMSG_FLAG_OPEN)
   {
      { // accessFlags
         unsigned accessFlagsLen;

         if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
            &accessFlags, &accessFlagsLen) )
            return false;

         bufPos += accessFlagsLen;
      }

      { // sessionID
         unsigned sessionBufLen;

         if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
            &sessionIDLen, &sessionID, &sessionBufLen) )
            return false;

         bufPos += sessionBufLen;
      }
   }

   if(this->intentFlags & LOOKUPINTENTMSG_FLAG_CREATE)
   {
      { // userID
         unsigned userIDLen;
         if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &userID, &userIDLen) )
            return false;

         bufPos += userIDLen;
      }

      { // groupID
         unsigned groupIDLen;
         if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &groupID, &groupIDLen) )
            return false;

         bufPos += groupIDLen;
      }

      { // mode
         unsigned modeLen;
         if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &mode, &modeLen) )
            return false;

         bufPos += modeLen;
      }

      { // preferredTargets
         if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
            &prefTargetsElemNum, &prefTargetsListStart, &prefTargetsBufLen) )
            return false;

         bufPos += prefTargetsBufLen;
      }
   }


   return true;
}

void LookupIntentMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // intentFlags
   bufPos += Serialization::serializeInt(&buf[bufPos], intentFlags);

   // parentInfo
   bufPos += this->parentInfoPtr->serialize(&buf[bufPos]);

   if(!this->intentFlags & LOOKUPINTENTMSG_FLAG_OPEN)
   {
      // entryName
      bufPos += Serialization::serializeStrAlign4(&buf[bufPos], this->entryName.length(),
         this->entryName.c_str() );
   }

   if(this->intentFlags & LOOKUPINTENTMSG_FLAG_REVALIDATE)
   {
      // entryInfo
      bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
   }

   if(this->intentFlags & LOOKUPINTENTMSG_FLAG_OPEN)
   {
      // accessFlags
      bufPos += Serialization::serializeUInt(&buf[bufPos], accessFlags);

      // sessionID
      bufPos += Serialization::serializeStrAlign4(&buf[bufPos], sessionIDLen, sessionID);
   }

   if(this->intentFlags & LOOKUPINTENTMSG_FLAG_CREATE)
   {
      // userID
      bufPos += Serialization::serializeUInt(&buf[bufPos], userID);

      // groupID
      bufPos += Serialization::serializeUInt(&buf[bufPos], groupID);

      // mode
      bufPos += Serialization::serializeInt(&buf[bufPos], mode);

      // preferredTargets
      bufPos += Serialization::serializeUInt16List(&buf[bufPos], preferredTargets);
   }

}




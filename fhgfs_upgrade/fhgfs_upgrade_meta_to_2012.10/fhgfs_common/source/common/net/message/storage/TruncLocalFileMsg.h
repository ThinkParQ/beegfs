#ifndef TRUNCLOCALFILEMSG_H_
#define TRUNCLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>


#define TRUNCLOCALFILEMSG_FLAG_NODYNAMICATTRIBS    1 /* skip retrieval of current dyn attribs */


class TruncLocalFileMsg : public NetMessage
{
   public:

      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       * @param targetID just a reference, so do not free it as long as you use this object!
       */
      TruncLocalFileMsg(int64_t filesize, std::string& entryID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_TruncLocalFile)
      {
         this->filesize = filesize;

         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();

         this->targetID = targetID;
         this->mirroredFromTargetID = 0;

         this->flags = 0;
      }
     
   protected:
      /**
       * For deserialization only!
       */
      TruncLocalFileMsg() : NetMessage(NETMSGTYPE_TruncLocalFile) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64() + // filesize
            Serialization::serialLenUInt() + // flags
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenUShort() + // mirroredFromTargetID
            Serialization::serialLenStrAlign4(entryIDLen);
      }


   private:
      int64_t filesize;

      unsigned entryIDLen;
      const char* entryID;

      uint16_t targetID;
      uint16_t mirroredFromTargetID; // chunk file is a mirrored version from this targetID

      unsigned flags; // TRUNCLOCALFILEMSG_FLAG_...


   public:
   
      // getters & setters

      int64_t getFilesize() const
      {
         return filesize;
      }
      
      const char* getEntryID() const
      {
         return entryID;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      uint16_t getMirroredFromTargetID() const
      {
         return mirroredFromTargetID;
      }

      void setMirroredFromTargetID(uint16_t mirroredFromTargetID)
      {
         this->mirroredFromTargetID = mirroredFromTargetID;
      }

      unsigned getFlags() const
      {
         return flags;
      }

      void setFlags(unsigned flags)
      {
         this->flags = flags;
      }

      // inliners

      /**
       * Test flag. (For convenience and readability.)
       *
       * @return !=0 if given flag is set.
       */
      unsigned isFlagSet(unsigned flag) const
      {
         return this->flags & flag;
      }


};

#endif /*TRUNCLOCALFILEMSG_H_*/

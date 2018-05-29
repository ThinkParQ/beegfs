#ifndef MKFILEWITHPATTERNMSG_H_
#define MKFILEWITHPATTERNMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/Path.h>

class MkFileWithPatternMsg : public MirroredMessageBase<MkFileWithPatternMsg>
{
   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param pattern just a reference, so do not free it as long as you use this object!
       */
      MkFileWithPatternMsg(const EntryInfo* parentInfo, const std::string& newFileName,
         const unsigned userID, const unsigned groupID, const int mode, const int umask,
         StripePattern* pattern)
          : BaseType(NETMSGTYPE_MkFileWithPattern),
            newFileName(newFileName.c_str()),
            newFileNameLen(newFileName.length()),
            userID(userID),
            groupID(groupID),
            mode(mode),
            umask(umask),
            parentInfoPtr(parentInfo),
            pattern(pattern)
      {
      }

      /**
       * Constructor for deserialization only
       */
      MkFileWithPatternMsg() : BaseType(NETMSGTYPE_MkFileWithPattern)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->userID
            % obj->groupID
            % obj->mode
            % obj->umask
            % serdes::backedPtr(obj->parentInfoPtr, obj->parentInfo)
            % serdes::rawString(obj->newFileName, obj->newFileNameLen, 4)
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern);
      }

   private:

      const char* newFileName;
      unsigned newFileNameLen;

      uint32_t userID;
      uint32_t groupID;
      int32_t mode;
      int32_t umask;

      // for serialization
      const EntryInfo* parentInfoPtr; // not owned by this object!
      StripePattern* pattern; // not owned by this object!

      // for deserialization
      EntryInfo parentInfo;
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;

   public:
      StripePattern& getPattern()
      {
         return *pattern;
      }

      // getters & setters
      unsigned getUserID() const
      {
         return userID;
      }

      unsigned getGroupID() const
      {
         return groupID;
      }

      int getMode() const
      {
         return mode;
      }

      int getUmask() const
      {
         return umask;
      }

      const EntryInfo* getParentInfo() const
      {
         return &this->parentInfo;
      }

      const char* getNewFileName() const
      {
         return this->newFileName;
      }

};

#endif /* MKFILEWITHPATTERNMSG_H_ */

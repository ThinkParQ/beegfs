#ifndef POSIXACL_H_
#define POSIXACL_H_

#include <common/toolkit/serialization/Serialization.h>
#include <common/Common.h>
#include <common/storage/StorageErrors.h>

#include <stdint.h>
#include <vector>

struct ACLEntry
{
   // Definitions to match linux/posix_acl.h and linux/posix_acl_xattr.h
   static const int32_t POSIX_ACL_XATTR_VERSION = 0x0002;
   enum Tag {
      ACL_USER_OBJ  = 0x01,
      ACL_USER      = 0x02,
      ACL_GROUP_OBJ = 0x04,
      ACL_GROUP     = 0x08,
      ACL_MASK      = 0x10,
      ACL_OTHER     = 0x20
   };

   int16_t tag;
   uint16_t perm;
   uint32_t id; // uid or gid depending on the tag

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->tag
         % obj->perm
         % obj->id;
   }
};

typedef std::vector<ACLEntry> ACLEntryVec;
typedef ACLEntryVec::const_iterator ACLEntryVecCIter;
typedef ACLEntryVec::iterator ACLEntryVecIter;

class PosixACL
{
   public:
      bool deserializeXAttr(const CharVector& xattr);
      void serializeXAttr(CharVector& xattr) const;

      std::string toString();
      FhgfsOpsErr modifyModeBits(int& outMode, bool& outNeedsACL);
      bool empty() { return entries.empty(); }

      static const std::string defaultACLXAttrName;
      static const std::string accessACLXAttrName;

   private:
      ACLEntryVec entries;
};

#endif /*POSIXACL_H_*/

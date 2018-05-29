#include "PosixACL.h"

const std::string PosixACL::defaultACLXAttrName = "system.posix_acl_default";
const std::string PosixACL::accessACLXAttrName = "system.posix_acl_access";

/**
 * @param xattr serialized form of the posix ACL to fill the PosixACL from.
 */
bool PosixACL::deserializeXAttr(const CharVector& xattr)
{
   Deserializer des(&xattr[0], xattr.size());

   {
      int32_t version;

      des % version;
      if (!des.good() || version != ACLEntry::POSIX_ACL_XATTR_VERSION)
         return false;
   }

   entries.clear();
   while (des.good() && des.size() < xattr.size())
   {
      ACLEntry newEntry;

      des % newEntry;
      entries.push_back(newEntry);
   }

   return des.good();
}

/**
 * Serialize the ACL in posix_acl_xattr form.
 */
void PosixACL::serializeXAttr(CharVector& xattr) const
{
   xattr.clear();

   // run twice: once with empty buffer, to calculate the size, then with proper buffer
   for (int i = 0; i < 2; i++)
   {
      Serializer ser(&xattr[0], xattr.size());

      ser % ACLEntry::POSIX_ACL_XATTR_VERSION;

      // entries
      for (ACLEntryVecCIter entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
         ser % *entryIt;

      xattr.resize(ser.size());
   }
}

/**
 * Modify the ACL based on the given mode bits. Also modifies the mode bits according to the
 * permissions granted in the ACL.
 * This effectively turns a directory default ACL into a file access ACL.
 *
 * @param mode Mode bits of the new file.
 * @returns whether an ACL is necessary for the newly created file.
 */
FhgfsOpsErr PosixACL::modifyModeBits(int& outMode, bool& outNeedsACL)
{
   outNeedsACL = false;
   int newMode = outMode;

   // Pointers to the group/mask mode of the ACL. In case we find group/mask entries, we have to
   // take them into account last.
   unsigned short* groupPerm = NULL;
   unsigned short* maskPerm = NULL;

   for (ACLEntryVecIter entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
   {
      ACLEntry& entry = *entryIt;

      switch (entry.tag)
      {
         case ACLEntry::ACL_USER_OBJ:
            {
               // Apply 'user' permission of the mode to the ACL's 'owner' entry.
               entry.perm &= (newMode >> 6) | ~0007;

               // Apply 'owner' entry of the ACL to the owner's permission in the mode flags.
               newMode &= (entry.perm << 6) | ~0700;
            }
            break;

         case ACLEntry::ACL_USER:
         case ACLEntry::ACL_GROUP:
            {
               // If the ACL has named user/group entries, it can't be represented using only
               // mode bits.
               outNeedsACL = true;
            }
            break;

         case ACLEntry::ACL_GROUP_OBJ:
            {
               groupPerm = &entry.perm;
            }
            break;

         case ACLEntry::ACL_OTHER:
            {
               // Apply 'other' permission from the mode to the ACL's 'other' entry.
               entry.perm &= newMode | ~0007;

               // Apply 'other' entry of the ACL to the 'other' permission in the mode flags.
               newMode &= entry.perm | ~0007;
            }
            break;

         case ACLEntry::ACL_MASK:
            {
               maskPerm = &entry.perm;
            }
            break;

         default:
            return FhgfsOpsErr_INTERNAL;
      }
   }

   if (maskPerm)
   {
      // The 'mask' entry of the ACL influences the 'group' access bits and vice-versa.
      *maskPerm &= (newMode >> 3) | ~0007;
      newMode &= (*maskPerm << 3) | ~0070;
   }
   else
   {
      // If there's no mask, the group mode bits are determined using the group acl entry.
      if (!groupPerm)
         return FhgfsOpsErr_INTERNAL;

      *groupPerm &= (newMode >> 3) | ~0007;
      newMode &= (*groupPerm << 3) | ~0070;
   }

   // Apply changes to the last nine mode bits.
   outMode = (outMode & ~0777) | newMode;

   return FhgfsOpsErr_SUCCESS;
}

std::string PosixACL::toString()
{
   std::ostringstream ostr;

   ostr << "ACL Size: " << entries.size() << std::endl;

   for (ACLEntryVecCIter it = entries.begin(); it != entries.end(); ++it)
      ostr << "Entry[ "
           << "tag: " << it->tag << " perm: " << it->perm << " id: " << it->id
           << " ]" << std::endl;

   return ostr.str();
}

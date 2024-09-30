#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/posix_acl_xattr.h>

#include "common/Common.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsHelper.h"

#include "FhgfsXAttrHandlers.h"
#define FHGFS_XATTR_USER_PREFIX "user."
#define FHGFS_XATTR_SECURITY_PREFIX "security."

#ifdef KERNEL_HAS_GET_ACL
/**
 * Called when an ACL Xattr is set. Responsible for setting the mode bits corresponding to the
 * ACL mask.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
static int FhgfsXAttrSetACL(const struct xattr_handler* handler, struct mnt_idmap* id_map,
   struct dentry* dentry, struct inode* inode, const char* name, const void* value, size_t size,
   int flags)
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
static int FhgfsXAttrSetACL(const struct xattr_handler* handler, struct user_namespace* mnt_userns,
   struct dentry* dentry, struct inode* inode, const char* name, const void* value, size_t size,
   int flags)
#else
static int FhgfsXAttrSetACL(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, const void* value, size_t size, int flags)
#endif
{
   int handler_flags = handler->flags;
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
static int FhgfsXAttrSetACL(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, const void* value, size_t size, int flags)
{
   int handler_flags = handler->flags;
   struct inode* inode = dentry->d_inode;
#else
static int FhgfsXAttrSetACL(struct dentry *dentry, const char *name, const void *value, size_t size,
      int flags, int handler_flags)
{
   struct inode* inode = dentry->d_inode;
#endif
   char* attrName;

   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__, "Called.");

   // Enforce an empty name here (which means the name of the Xattr has to be
   // fully given by the POSIX_ACL_XATTR_... defines)
   if(strcmp(name, "") )
      return -EINVAL;

   if(!os_inode_owner_or_capable(inode) )
      return -EPERM;

   if(S_ISLNK(inode->i_mode) )
      return -EOPNOTSUPP;

   if(handler_flags == ACL_TYPE_ACCESS)
   {
      struct posix_acl* acl;
      struct iattr attr;
      int aclEquivRes;
      int setAttrRes;

      // if we set the access ACL, we also need to update the file mode permission bits.
      attr.ia_mode = inode->i_mode;
      attr.ia_valid = ATTR_MODE;

      acl = os_posix_acl_from_xattr(value, size);

      if(IS_ERR(acl) )
         return PTR_ERR(acl);

      aclEquivRes = posix_acl_equiv_mode(acl, &attr.ia_mode);
      if(aclEquivRes == 0) // ACL can be exactly represented by file mode permission bits
      {
         value = NULL;
      }
      else if(aclEquivRes < 0)
      {
         posix_acl_release(acl);
         return -EINVAL;
      }

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
      setAttrRes = FhgfsOps_setattr(&nop_mnt_idmap, dentry, &attr);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
      setAttrRes = FhgfsOps_setattr(&init_user_ns, dentry, &attr);
#else
      setAttrRes = FhgfsOps_setattr(dentry, &attr);
#endif
      if(setAttrRes < 0)
         return setAttrRes;

      posix_acl_release(acl);

      // Name of the Xattr to be set later
      attrName = XATTR_NAME_POSIX_ACL_ACCESS;
   }
   else if(handler_flags == ACL_TYPE_DEFAULT)
   {
      // Note: The default acl is not reflected in any file mode permission bits.
      // Just check for correctness here, and delete the xattr if the acl is empty.
      struct posix_acl* acl;
      acl = os_posix_acl_from_xattr(value, size);

      if (IS_ERR(acl))
         return PTR_ERR(acl);

      if (acl == NULL)
         value = NULL;
      else
         posix_acl_release(acl);

      attrName = XATTR_NAME_POSIX_ACL_DEFAULT;
   }
   else
      return -EOPNOTSUPP;

   if(value)
      return FhgfsOps_setxattr(dentry, attrName, value, size, flags);
   else // value == NULL: Remove the ACL extended attribute.
   {
      int removeRes = FhgfsOps_removexattr(dentry, attrName);
      if (removeRes == 0 || removeRes == -ENODATA) // If XA didn't exist anyway, return 0.
         return 0;
      else
         return removeRes;
   }

}

/**
 * The get-function of the xattr handler which handles the XATTR_NAME_POSIX_ACL_ACCESS and
 * XATTR_NAME_POSIX_ACL_DEFAULT xattrs.
 * @param name has to be a pointer to an empty string ("").
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
static int FhgfsXAttrGetACL(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, void* value, size_t size)
{
   int handler_flags = handler->flags;
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
static int FhgfsXAttrGetACL(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, void* value, size_t size)
{
   int handler_flags = handler->flags;
#else
int FhgfsXAttrGetACL(struct dentry* dentry, const char* name, void* value, size_t size,
      int handler_flags)
{
#endif
   char* attrName;

   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__, "Called.");

   // For simplicity we enforce an empty name here (which means the name of the Xattr has to be
   // fully given by the POSIX_ACL_XATTR_... defines)
   if(strcmp(name, "") )
      return -EINVAL;

   if(handler_flags == ACL_TYPE_ACCESS)
      attrName = XATTR_NAME_POSIX_ACL_ACCESS;
   else if(handler_flags == ACL_TYPE_DEFAULT)
      attrName = XATTR_NAME_POSIX_ACL_DEFAULT;
   else
      return -EOPNOTSUPP;

   return FhgfsOps_getxattr(dentry, attrName, value, size);
}
#endif // KERNEL_HAS_GET_ACL

/**
 * The get-function which is used for all the user.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
static int FhgfsXAttr_getUser(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, void* value, size_t size)
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
static int FhgfsXAttr_getUser(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, void* value, size_t size)
#elif defined(KERNEL_HAS_DENTRY_XATTR_HANDLER)
static int FhgfsXAttr_getUser(struct dentry* dentry, const char* name, void* value, size_t size,
      int handler_flags)
#else
static int FhgfsXAttr_getUser(struct inode* inode, const char* name, void* value, size_t size)
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_USER_PREFIX) );
   // Note: strlen does not count the terminating '\0', but sizeof does. So we have space for
   // exactly one '\0' which coincidally is just what we need.

#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s; size: %u)", name, size);
#else
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(inode->i_sb), NULL, inode, __func__,
      "(name: %s; size: %u)", name, size);
#endif

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;

   strcpy(prefixedName, FHGFS_XATTR_USER_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_USER_PREFIX) - 1, name); // sizeof-1 to remove the '\0'

#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
   res = FhgfsOps_getxattr(dentry, prefixedName, value, size);
#else
   res = FhgfsOps_getxattr(inode, prefixedName, value, size);
#endif

   kfree(prefixedName);
   return res;
}

/**
 * The set-function which is used for all the user.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
static int FhgfsXAttr_setUser(const struct xattr_handler* handler, struct mnt_idmap* id_map,
   struct dentry* dentry, struct inode* inode, const char* name, const void* value, size_t size,
   int flags)
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
static int FhgfsXAttr_setUser(const struct xattr_handler* handler, struct user_namespace* mnt_userns,
   struct dentry* dentry, struct inode* inode, const char* name, const void* value, size_t size,
   int flags)
#else
static int FhgfsXAttr_setUser(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, const void* value, size_t size, int flags)
#endif

#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
static int FhgfsXAttr_setUser(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, const void* value, size_t size, int flags)
#elif defined(KERNEL_HAS_DENTRY_XATTR_HANDLER)
static int FhgfsXAttr_setUser(struct dentry* dentry, const char* name, const void* value, size_t size,
   int flags, int handler_flags)
#endif // KERNEL_HAS_DENTRY_XATTR_HANDLER
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_USER_PREFIX) );


#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s)", name);
#else
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(inode->i_sb), NULL, inode, __func__,
      "(name: %s)", name);
#endif // KERNEL_HAS_DENTRY_XATTR_HANDLER

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;
   strcpy(prefixedName, FHGFS_XATTR_USER_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_USER_PREFIX) - 1, name); // sizeof-1 to remove the '\0'

   if (value)
   {
#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
      res = FhgfsOps_setxattr(dentry, prefixedName, value, size, flags);
#else
      res = FhgfsOps_setxattr(inode, prefixedName, value, size, flags);
#endif // KERNEL_HAS_DENTRY_XATTR_HANDLER
   }
   else
   {
      res = FhgfsOps_removexattr(dentry, prefixedName);
   }

   kfree(prefixedName);
   return res;
}


/**
 * The get-function which is used for all the security.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
static int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, void* value, size_t size)
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
static int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, void* value, size_t size)
#elif defined(KERNEL_HAS_DENTRY_XATTR_HANDLER)
static int FhgfsXAttr_getSecurity(struct dentry* dentry, const char* name, void* value, size_t size,
      int handler_flags)
#else
static int FhgfsXAttr_getSecurity(struct inode* inode, const char* name, void* value, size_t size)
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_SECURITY_PREFIX) );
   // Note: strlen does not count the terminating '\0', but sizeof does. So we have space for
   // exactly one '\0' which coincidally is just what we need.

#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s; size: %u)", name, size);
#else
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(inode->i_sb), NULL, inode, __func__,
      "(name: %s; size: %u)", name, size);
#endif // KERNEL_HAS_DENTRY_XATTR_HANDLER

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;

   strcpy(prefixedName, FHGFS_XATTR_SECURITY_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_SECURITY_PREFIX) - 1, name); // sizeof-1 to remove '\0'

#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
   res = FhgfsOps_getxattr(dentry, prefixedName, value, size);
#else
   res = FhgfsOps_getxattr(inode, prefixedName, value, size);
#endif // KERNEL_HAS_DENTRY_XATTR_HANDLER

   kfree(prefixedName);
   return res;
}

/**
 * The set-function which is used for all the security.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
static int FhgfsXAttr_setSecurity(const struct xattr_handler* handler, struct mnt_idmap* idmap,
   struct dentry* dentry, struct inode* inode, const char* name, const void* value, size_t size,
   int flags)
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
static int FhgfsXAttr_setSecurity(const struct xattr_handler* handler, struct user_namespace* mnt_userns,
   struct dentry* dentry, struct inode* inode, const char* name, const void* value, size_t size,
   int flags)
#else
static int FhgfsXAttr_setSecurity(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, const void* value, size_t size, int flags)
#endif
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
static int FhgfsXAttr_setSecurity(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, const void* value, size_t size, int flags)
#elif defined(KERNEL_HAS_DENTRY_XATTR_HANDLER)
static int FhgfsXAttr_setSecurity(struct dentry* dentry, const char* name, const void* value, size_t size,
   int flags, int handler_flags)
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_SECURITY_PREFIX) );


#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s)", name);
#else
   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(inode->i_sb), NULL, inode, __func__,
      "(name: %s)", name);
#endif

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;
   strcpy(prefixedName, FHGFS_XATTR_SECURITY_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_SECURITY_PREFIX) - 1, name); // sizeof-1 to remove '\0'

   if (value)
   {
#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
      res = FhgfsOps_setxattr(dentry, prefixedName, value, size, flags);
#else
      res = FhgfsOps_setxattr(inode, prefixedName, value, size, flags);
#endif
   }
   else
   {
      res = FhgfsOps_removexattr(dentry, prefixedName);
   }

   kfree(prefixedName);
   return res;
}

#ifdef KERNEL_HAS_GET_ACL
struct xattr_handler fhgfs_xattr_acl_access_handler =
{
#ifdef KERNEL_HAS_XATTR_HANDLER_NAME
   .name = XATTR_NAME_POSIX_ACL_ACCESS,
#else
   .prefix = XATTR_NAME_POSIX_ACL_ACCESS,
#endif
   .flags  = ACL_TYPE_ACCESS,
   .list   = NULL,
   .get    = FhgfsXAttrGetACL,
   .set    = FhgfsXAttrSetACL,
};

struct xattr_handler fhgfs_xattr_acl_default_handler =
{
#ifdef KERNEL_HAS_XATTR_HANDLER_NAME
   .name   = XATTR_NAME_POSIX_ACL_DEFAULT,
#else
   .prefix = XATTR_NAME_POSIX_ACL_DEFAULT,
#endif
   .flags  = ACL_TYPE_DEFAULT,
   .list   = NULL,
   .get    = FhgfsXAttrGetACL,
   .set    = FhgfsXAttrSetACL,
};
#endif // KERNEL_HAS_GET_ACL

struct xattr_handler fhgfs_xattr_user_handler =
{
   .prefix = FHGFS_XATTR_USER_PREFIX,
   .list   = NULL,
   .set    = FhgfsXAttr_setUser,
   .get    = FhgfsXAttr_getUser,
};

struct xattr_handler fhgfs_xattr_security_handler =
{
   .prefix = FHGFS_XATTR_SECURITY_PREFIX,
   .list   = NULL,
   .set    = FhgfsXAttr_setSecurity,
   .get    = FhgfsXAttr_getSecurity,
};

#if defined(KERNEL_HAS_CONST_XATTR_CONST_PTR_HANDLER)
const struct xattr_handler* const fhgfs_xattr_handlers[] =
#elif defined(KERNEL_HAS_CONST_XATTR_HANDLER)
const struct xattr_handler* fhgfs_xattr_handlers[] =
#else
struct xattr_handler* fhgfs_xattr_handlers[] =
#endif
{
#ifdef KERNEL_HAS_GET_ACL
   &fhgfs_xattr_acl_access_handler,
   &fhgfs_xattr_acl_default_handler,
#endif
   &fhgfs_xattr_user_handler,
   &fhgfs_xattr_security_handler,
   NULL
};

#if defined(KERNEL_HAS_CONST_XATTR_CONST_PTR_HANDLER)
const struct xattr_handler* const fhgfs_xattr_handlers_noacl[] =
#elif defined(KERNEL_HAS_CONST_XATTR_HANDLER)
const struct xattr_handler* fhgfs_xattr_handlers_noacl[] =
#else
struct xattr_handler* fhgfs_xattr_handlers_noacl[] =
#endif
{
   &fhgfs_xattr_user_handler,
   &fhgfs_xattr_security_handler,
   NULL
};


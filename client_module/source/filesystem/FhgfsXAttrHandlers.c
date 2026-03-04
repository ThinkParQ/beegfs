#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/posix_acl_xattr.h>

#include "common/Common.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsHelper.h"
#include "FhgfsXAttrHandlers.h"

#define FHGFS_XATTR_USER_PREFIX "user."
#define FHGFS_XATTR_SECURITY_PREFIX "security."

/* Extended Attribute (xattr) name index constants for BeeGFS */
#define BEEGFS_XATTR_INDEX_USER                    1 /* User namespace (user.*) */
#define BEEGFS_XATTR_INDEX_POSIX_ACL_ACCESS        2 /* POSIX ACL access (system.posix_acl_access) */
#define BEEGFS_XATTR_INDEX_POSIX_ACL_DEFAULT       3 /* POSIX ACL default (system.posix_acl_default) */
#define BEEGFS_XATTR_INDEX_TRUSTED                 4 /* Trusted namespace (trusted.*) */
#define BEEGFS_XATTR_INDEX_SECURITY                5 /* Security namespace (security.*) */
#define BEEGFS_XATTR_INDEX_SYSTEM                  6 /* System namespace (system.*) */


#ifdef KERNEL_HAS_GET_ACL
/**
 * Called when an ACL Xattr is set. Responsible for setting the mode bits corresponding to the
 * ACL mask.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
static int FhgfsXAttrSetACL(const struct xattr_handler* handler, struct mnt_idmap* idmap,
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

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
   if (!inode_owner_or_capable(idmap, inode))
      return -EPERM;
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
   if (!inode_owner_or_capable(mnt_userns, inode))
      return -EPERM;
#else
   if (!inode_owner_or_capable(inode))
      return -EPERM;
#endif

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

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
      acl = os_posix_acl_from_xattr(inode->i_sb->s_user_ns, value, size);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
      acl = os_posix_acl_from_xattr(mnt_userns, value, size);
#else
      acl = os_posix_acl_from_xattr(&init_user_ns, value, size);
#endif

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
      setAttrRes = FhgfsOps_setattr(idmap, dentry, &attr);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
      setAttrRes = FhgfsOps_setattr(mnt_userns, dentry, &attr);
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
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
      acl = os_posix_acl_from_xattr(inode->i_sb->s_user_ns, value, size);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
      acl = os_posix_acl_from_xattr(mnt_userns, value, size);
#else
      acl = os_posix_acl_from_xattr(&init_user_ns, value, size);
#endif

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
      return FhgfsOps_setxattr(dentry->d_inode, attrName, value, size, flags);
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

   return FhgfsOps_getxattr(dentry->d_inode, attrName, value, size);
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
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_USER_PREFIX) );
   // Note: strlen does not count the terminating '\0', but sizeof does. So we have space for
   // exactly one '\0' which coincidally is just what we need.

   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s; size: %u)", name, size);

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;

   strcpy(prefixedName, FHGFS_XATTR_USER_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_USER_PREFIX) - 1, name); // sizeof-1 to remove the '\0'

   res = FhgfsOps_getxattr(dentry->d_inode, prefixedName, value, size);

   kfree(prefixedName);
   return res;
}

/**
 * The set-function which is used for all the user.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
static int FhgfsXAttr_setUser(const struct xattr_handler* handler, struct mnt_idmap* idmap,
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
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_USER_PREFIX) );


   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s)", name);

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;
   strcpy(prefixedName, FHGFS_XATTR_USER_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_USER_PREFIX) - 1, name); // sizeof-1 to remove the '\0'

   if (value)
   {
      res = FhgfsOps_setxattr(dentry->d_inode, prefixedName, value, size, flags);
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
int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
   struct inode* inode, const char* name, void* value, size_t size)
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
   const char* name, void* value, size_t size)
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_SECURITY_PREFIX) );
   // Note: strlen does not count the terminating '\0', but sizeof does. So we have space for
   // exactly one '\0' which coincidally is just what we need.

   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;

   strcpy(prefixedName, FHGFS_XATTR_SECURITY_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_SECURITY_PREFIX) - 1, name); // sizeof-1 to remove '\0'

#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
   res = FhgfsOps_getxattr(inode, prefixedName, value, size);
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
   res = FhgfsOps_getxattr(dentry->d_inode, prefixedName, value, size);
#endif

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
#endif
{
   FhgfsOpsErr res;
   char* prefixedName = os_kmalloc(strlen(name) + sizeof(FHGFS_XATTR_SECURITY_PREFIX) );

   FhgfsOpsHelper_logOpDebug(FhgfsOps_getApp(dentry->d_sb), dentry, NULL, __func__,
      "(name: %s)", name);
   // add name prefix which has been removed by the generic function
   if(!prefixedName)
      return -ENOMEM;
   strcpy(prefixedName, FHGFS_XATTR_SECURITY_PREFIX);
   strcpy(prefixedName + sizeof(FHGFS_XATTR_SECURITY_PREFIX) - 1, name); // sizeof-1 to remove '\0'

   if (value)
   {
      res = FhgfsOps_setxattr(inode, prefixedName, value, size, flags);
   }
   else
   {
      res = FhgfsOps_removexattr(dentry, prefixedName);
   }

   kfree(prefixedName);
   return res;
}


/**
 * beegfs_xattr_set - Set, replace, or remove an extended attribute.
 * @inode:      Inode to modify.
 * @name_index: Namespace index.
 * @name:       Xattr name (without prefix).
 * @value:      New value to set, or NULL to remove.
 * @value_len:  Length of the value.
 * @flags:      XATTR_CREATE, XATTR_REPLACE, or 0.
 *
 * This function formats the full extended attribute name by adding a
 * namespace prefix (e.g., "user.", "security.", "trusted.") and performs
 * the appropriate action (create, replace, or remove).
 *
 * NOTE:
 * This implementation avoids a pre-check RPC (e.g., getxattr) by delegating
 * validation to the metadata server. The metadata handler is expected to:
 *   1. Check if the attribute already exists.
 *   2. If XATTR_CREATE is set and the attribute exists, return -EEXIST.
 *   3. If XATTR_REPLACE is set and the attribute does not exist, return -ENODATA.
 *   4. If @value is NULL, remove the xattr (equivalent to removexattr).
 *
 * This reduces round-trips to the meta node and improves performance.
 *
 * Returns:
 *   0 on success,
 *   -EINVAL for invalid arguments,
 *   -ERANGE if name is too long,
 *   -EEXIST if XATTR_CREATE fails due to existing xattr,
 *   -ENODATA if XATTR_REPLACE fails due to missing xattr,
 *   -EOPNOTSUPP for unsupported xattr namespace.
 */
static int beegfs_xattr_set(struct inode *inode, int name_index,
      const char *name, const void *value, size_t value_len,
      int flags)
{
   char prefixed_name[XATTR_NAME_MAX];

   if (!name)
      return -EINVAL;

   if (strlen(name) > 255)
      return -ERANGE;

   // Add appropriate namespace prefix to the name
   switch (name_index) {
      case BEEGFS_XATTR_INDEX_USER:
         snprintf(prefixed_name, sizeof(prefixed_name), "user.%s", name);
         break;
      case BEEGFS_XATTR_INDEX_SECURITY:
         snprintf(prefixed_name, sizeof(prefixed_name), "security.%s", name);
         break;
      case BEEGFS_XATTR_INDEX_TRUSTED:
         snprintf(prefixed_name, sizeof(prefixed_name), "trusted.%s", name);
         break;
      default:
         return -EOPNOTSUPP; // Unsupported namespace
   }

   /* If value is NULL, remove xattr */
   if (!value)
      return FhgfsOps_removexattrInode(inode, prefixed_name);

   if ((flags & XATTR_CREATE) && (flags & XATTR_REPLACE))
      return -EINVAL;

   return FhgfsOps_setxattr(inode, prefixed_name, value, value_len, flags);
}

/**
 * FhgfsXAttr_initxattrs - Helper for initializing default xattrs on inode creation.
 * @inode:        Inode to initialize.
 * @xattr_array:  Array of xattrs to apply.
 * @fs_info:      Not used (for compatibility).
 *
 * Returns 0 on success or the first encountered error.
 */
static int FhgfsXAttr_initxattrs(struct inode *inode, const struct xattr *xattr_array,
      void *fs_info)
{
   const struct xattr *xattr;
   int err = 0;

   if (!inode || !inode->i_sb) {
      return -EINVAL;
   }

   // Iterate over the xattr array and apply each one in the "security" namespace
   for (xattr = xattr_array; xattr->name != NULL; xattr++) {
      err = beegfs_xattr_set(inode,
            BEEGFS_XATTR_INDEX_SECURITY,
            xattr->name, xattr->value,
            xattr->value_len, XATTR_CREATE);
      if (err < 0)
         break;
   }
   return err;
}

/**
 * FhgfsXAttr_security_xattr_enabled - Check if security xattrs are enabled for the given inode.
 * @inode: Pointer to inode to check.
 *
 * Returns true if both inode and superblock have security fields initialized.
 */
static inline int FhgfsXAttr_security_xattr_enabled(struct inode *inode)
{
   if (!inode || !inode->i_sb) {
      return 0;
   }
   return inode->i_security && inode->i_sb->s_security;
}


/**
 * FhgfsXAttr_init_security - Call LSM[Linux Security Module] hook to initialize security xattrs.
 * @inode: Inode being created.
 * @dir:   Directory in which inode is created.
 * @qstr:  Filename for the new inode.
 *
 * Returns 0 on success or a negative error from the LSM.
 */
int FhgfsXAttr_init_security(struct inode *inode, struct inode *dir,
      const struct qstr *qstr)
{
   if (!dir) {
      return -EINVAL;
   }
   if (!FhgfsXAttr_security_xattr_enabled(dir)) {
      printk_fhgfs_debug(KERN_WARNING, "BeeGFS: Operation not supported (returned -EOPNOTSUPP)."
            "Please ensure SELinux is properly configured and enabled.\n");
      return -EOPNOTSUPP;
   }

   return security_inode_init_security(inode, dir, qstr,
         &FhgfsXAttr_initxattrs, NULL);
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
   &fhgfs_xattr_security_handler,
   &fhgfs_xattr_user_handler,
   NULL
};

#if defined(KERNEL_HAS_CONST_XATTR_CONST_PTR_HANDLER)
const struct xattr_handler* const fhgfs_xattr_handlers_acl[] =
#elif defined(KERNEL_HAS_CONST_XATTR_HANDLER)
const struct xattr_handler* fhgfs_xattr_handlers_acl[] =
#else
struct xattr_handler* fhgfs_xattr_handlers_acl[] =
#endif
{
#ifdef KERNEL_HAS_GET_ACL
   &fhgfs_xattr_acl_access_handler,
   &fhgfs_xattr_acl_default_handler,
#endif
   &fhgfs_xattr_user_handler,
   NULL
};

#if defined(KERNEL_HAS_CONST_XATTR_CONST_PTR_HANDLER)
const struct xattr_handler* const fhgfs_xattr_handlers_selinux[] =
#elif defined(KERNEL_HAS_CONST_XATTR_HANDLER)
const struct xattr_handler* fhgfs_xattr_handlers_selinux[] =
#else
struct xattr_handler* fhgfs_xattr_handlers_selinux[] =
#endif
{
   &fhgfs_xattr_security_handler,
   &fhgfs_xattr_user_handler,
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
   NULL
};


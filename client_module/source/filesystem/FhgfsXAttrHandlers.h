#ifndef FHGFSXATTRHANDLERS_H_
#define FHGFSXATTRHANDLERS_H_

#include <linux/xattr.h>

#ifdef KERNEL_HAS_CONST_XATTR_HANDLER
#ifdef KERNEL_HAS_GET_ACL
extern const struct xattr_handler* fhgfs_xattr_handlers[];
#endif
extern const struct xattr_handler* fhgfs_xattr_handlers_noacl[];
#else
#ifdef KERNEL_HAS_GET_ACL
extern struct xattr_handler* fhgfs_xattr_handlers[];
#endif
extern struct xattr_handler* fhgfs_xattr_handlers_noacl[];
#endif // KERNEL_HAS_CONST_XATTR_HANDLER


#endif /* FHGFSXATTRHANDLERS_H_ */

#ifndef STORAGEERRORS_H_
#define STORAGEERRORS_H_

/*
 * Remember to keep these definitions in sync with StorageErrors.h of fhgfs_common!
 */


#include <common/Common.h>


// Note: keep this above __FHGFSOPS_ERRLIST declaration
struct FhgfsOpsErrListEntry
{
   const char* errString; // human-readable error string
   int         sysErr;    // positive linux system error code
};


extern struct FhgfsOpsErrListEntry const __FHGFSOPS_ERRLIST[];


/**
 * Note: Remember to keep this in sync with FHGFSOPS_ERRLIST
 *
 * Note: We need the negative dummy (-1) because some return values (like CommKit) cast this enum to
 * negative int64_t and this leads bad (positive) values when the enum isn't signed. So the dummy
 * forces the compiler to make the enum a signed variable.
 */
enum FhgfsOpsErr
{
   FhgfsOpsErr_DUMMY_DONTUSEME         =    -1, /* see comment above */
   FhgfsOpsErr_SUCCESS                 =     0,
   FhgfsOpsErr_INTERNAL                =     1,
   FhgfsOpsErr_INTERRUPTED             =     2,
   FhgfsOpsErr_COMMUNICATION           =     3,
   FhgfsOpsErr_COMMTIMEDOUT            =     4,
   FhgfsOpsErr_UNKNOWNNODE             =     5,
   FhgfsOpsErr_NOTOWNER                =     6,
   FhgfsOpsErr_EXISTS                  =     7,
   FhgfsOpsErr_PATHNOTEXISTS           =     8,
   FhgfsOpsErr_INUSE                   =     9,
   FhgfsOpsErr_DYNAMICATTRIBSOUTDATED  =    10,
   FhgfsOpsErr_PARENTTOSUBDIR          =    11,
   FhgfsOpsErr_NOTADIR                 =    12,
   FhgfsOpsErr_NOTEMPTY                =    13,
   FhgfsOpsErr_NOSPACE                 =    14,
   FhgfsOpsErr_UNKNOWNTARGET           =    15,
   FhgfsOpsErr_WOULDBLOCK              =    16,
   FhgfsOpsErr_INODENOTINLINED         =    17, // inode is not inlined into the dentry
   FhgfsOpsErr_SAVEERROR               =    18, // saving to the underlying file system failed
   FhgfsOpsErr_TOOBIG                  =    19, // corresponds to EFBIG
   FhgfsOpsErr_INVAL                   =    20, // corresponds to EINVAL
   FhgfsOpsErr_ADDRESSFAULT            =    21, // corresponds to EFAULT
   FhgfsOpsErr_AGAIN                   =    22, // corresponds to EAGAIN
   FhgfsOpsErr_STORAGE_SRV_CRASHED     =    23, /* Potential cache loss for open file handle.
                                                   (Server crash detected.)*/
   FhgfsOpsErr_PERM                    =    24, // corresponds to EPERM
   FhgfsOpsErr_DQUOT                   =    25, // corresponds to EDQUOT (quota exceeded)
   FhgfsOpsErr_OUTOFMEM                =    26, // corresponds to ENOMEM (mem allocation failed)
   FhgfsOpsErr_RANGE                   =    27, // corresponds to ERANGE (needed for xattrs)
   FhgfsOpsErr_NODATA                  =    28, // corresponds to ENODATA==ENOATTR (xattr not found)
   FhgfsOpsErr_NOTSUPP                 =    29, // corresponds to EOPNOTSUPP
   FhgfsOpsErr_TOOLONG                 =    30, // corresponds to E2BIG (needed for xattrs)
};
typedef enum FhgfsOpsErr FhgfsOpsErr;



extern const char* FhgfsOpsErr_toErrString(FhgfsOpsErr fhgfsOpsErr);
extern int FhgfsOpsErr_toSysErr(FhgfsOpsErr fhgfsOpsErr);




#endif /*STORAGEERRORS_H_*/

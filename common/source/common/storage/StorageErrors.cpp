#include <common/app/log/LogContext.h>
#include "StorageErrors.h"

#include <boost/io/ios_state.hpp>

/**
 * Note: This is based on the FhgfsOpsErr entries
 * Note: We use EREMOTEIO as a generic error here
 */
struct FhgfsOpsErrListEntry const __FHGFSOPS_ERRLIST[] =
{
   {"Success", 0}, // FhgfsOpsErr_SUCCESS
   {"Internal error", EREMOTEIO}, // FhgfsOpsErr_INTERNAL
   {"Interrupted system call", EINTR}, // FhgfsOpsErr_INTERRUPTED
   {"Communication error", ECOMM}, // FhgfsOpsErr_COMMUNICATION
   {"Communication timeout", ETIMEDOUT}, // FhgfsOpsErr_COMMTIMEDOUT
   {"Unknown node", EREMOTEIO}, // FhgfsOpsErr_UNKNOWNNODE
   {"Node is not owner of entry", EREMOTEIO}, // FhgfsOpsErr_NOTOWNER
   {"Entry exists already", EEXIST}, // FhgfsOpsErr_EXISTS
   {"Path does not exist", ENOENT}, // FhgfsOpsErr_PATHNOTEXISTS
   {"Entry is in use", EBUSY}, // FhgfsOpsErr_INUSE
   {"Dynamic attributes of entry are outdated", EREMOTEIO}, // FhgfsOpsErr_INUSE
   {"Removed", 999}, // former FhgfsOpsErr_PARENTTOSUBDIR, not used
   {"Entry is not a directory", ENOTDIR}, // FhgfsOpsErr_NOTADIR
   {"Directory is not empty", ENOTEMPTY}, // FhgfsOpsErr_NOTEMPTY
   {"No space left", ENOSPC}, // FhgfsOpsErr_NOSPACE
   {"Unknown storage target", EREMOTEIO}, // FhgfsOpsErr_UNKNOWNTARGET
   {"Operation would block", EWOULDBLOCK}, // FhgfsOpsErr_WOULDBLOCK
   {"Inode not inlined", EREMOTEIO}, // FhgfsOpsErr_INODENOTINLINED
   {"Underlying file system error", EREMOTEIO}, // FhgfsOpsErr_SAVEERROR
   {"Argument too large", EFBIG},  // FhgfsOpsErr_TOOBIG
   {"Invalid argument", EINVAL},   // FhgfsOpsErr_INVAL
   {"Bad memory address", EFAULT}, // FhgfsOpsErr_ADDRESSFAULT
   {"Try again", EAGAIN},          // FhgfsOpsErr_AGAIN
   {"Potential cache loss for open file handle. (Server crash detected.)" , EREMOTEIO}, /*
                                                                  FhgfsOpsErr_STORAGE_SRV_CRASHED*/
   {"Permission denied", EPERM},   // FhgfsOpsErr_PERM
   {"Quota exceeded", EDQUOT},     // FhgfsOpsErr_DQUOT
   {"Out of memory", ENOMEM},      // FhgfsOpsErr_OUTOFMEM
   {"Numerical result out of range", ERANGE}, // FhgfsOpsErr_RANGE
   {"No data available", ENODATA}, // FhgfsOpsErr_NODATA
   {"Operation not supported", EOPNOTSUPP}, // FhgfsOpsErr_NOTSUPP
   {"Unknown storage pool", EINVAL},  // FhgfsOpsErr_UNKNOWNPOOL
   {NULL, 0}
};


std::ostream& operator<<(std::ostream& os, FhgfsOpsErr errCode)
{
   size_t unsignedErrCode = (size_t)errCode;

   if(likely(unsignedErrCode < __FHGFSOPS_ERRLIST_SIZE) )
      return os << __FHGFSOPS_ERRLIST[errCode].errString;

   // error code unknown...

   LogContext log("FhgfsOpsErrTk::toErrString");

   log.log(Log_CRITICAL, "Unknown errCode given: " +
      StringTk::intToStr(errCode) + "/" + StringTk::uintToStr(errCode) + " "
      "(dumping stack...)");

   log.logBacktrace();

   const boost::io::ios_all_saver ifs(os);

   os.flags(std::ios_base::dec);
   os.width(0);

   return os << "Unknown error code " << int(errCode);
}

/**
 * @return positive linux error code
 */
int FhgfsOpsErrTk::toSysErr(FhgfsOpsErr errCode)
{
   size_t unsignedErrCode = (size_t)errCode;

   if(likely(unsignedErrCode < __FHGFSOPS_ERRLIST_SIZE) )
      return __FHGFSOPS_ERRLIST[errCode].sysErr;

   // error code unknown...

   LogContext log("FhgfsOpsErrTk::toSysErr");

   log.log(Log_CRITICAL, "Unknown errCode given: " +
      StringTk::intToStr(errCode) + "/" + StringTk::uintToStr(errCode) + " "
      "(dumping stack...)");

   log.logBacktrace();

   return EPERM;
}

/**
 * @param errCode positive(!) system error code
 * Note: Since FhgfsOpsErr <-> sysErr is not a one-to-one mapping, only some error codes are
 *       available here. Feel free to add more!
 */
FhgfsOpsErr FhgfsOpsErrTk::fromSysErr(const int errCode)
{
   for (auto* entry = __FHGFSOPS_ERRLIST; entry->errString; entry++)
   {
      if (entry->sysErr == errCode)
         return FhgfsOpsErr(entry - __FHGFSOPS_ERRLIST);
   }

   return FhgfsOpsErr_INTERNAL;
}

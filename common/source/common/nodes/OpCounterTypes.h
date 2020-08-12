/*
 * OpCounterTypes.h - enum definitions and enum to name mapping
 *
 */

#ifndef OPCOUNTERTYPES_H_
#define OPCOUNTERTYPES_H_

#include <common/toolkit/StringTk.h>

/**
 * Per-client metadata server operation counters.
 */
enum MetaOpCounterTypes
{
   MetaOpCounter_OPSUM = 0, // sum of all ops, simplifies search of nodes with most operations

   MetaOpCounter_ACK,
   MetaOpCounter_CLOSE,
   MetaOpCounter_GETENTRYINFO,
   MetaOpCounter_FINDOWNER,
   MetaOpCounter_MKDIR,
   MetaOpCounter_MKFILE,
   MetaOpCounter_READDIR,
   MetaOpCounter_REFRESHENTRYINFO,
   MetaOpCounter_REQUESTMETADATA,
   MetaOpCounter_RMDIR,
   MetaOpCounter_RMLINK,
   MetaOpCounter_MOVINGDIRINSERT,
   MetaOpCounter_MOVINGFILEINSERT,
   MetaOpCounter_OPEN,
   MetaOpCounter_RENAME,
   MetaOpCounter_SETCHANNELDIRECT,
   MetaOpCounter_SETATTR,
   MetaOpCounter_SETDIRPATTERN,
   MetaOpCounter_STAT,
   MetaOpCounter_STATFS,  // StatStoragePath
   MetaOpCounter_TRUNCATE,
   MetaOpCounter_SYMLINK,
   MetaOpCounter_UNLINK,
   MetaOpCounter_LOOKUPINTENT_SIMPLE, // LookupIntentMsg without extra flags
   MetaOpCounter_LOOKUPINTENT_STAT,   // LookupIntentMsg with stat flag
   MetaOpCounter_LOOKUPINTENT_REVALIDATE, // LookupIntentMsg with revalidate flag (overrides stat)
   MetaOpCounter_LOOKUPINTENT_OPEN,   // LookupIntentMsg with open flag (overrides reval and stat)
   MetaOpCounter_LOOKUPINTENT_CREATE, // LookupIntentMsg with create flag (overrides other flags)
   MetaOpCounter_HARDLINK,
   MetaOpCounter_FLOCKAPPEND,
   MetaOpCounter_FLOCKENTRY,
   MetaOpCounter_FLOCKRANGE,
   MetaOpCounter_UPDATEDIRPARENT,
   MetaOpCounter_LISTXATTR,
   MetaOpCounter_GETXATTR,
   MetaOpCounter_REMOVEXATTR,
   MetaOpCounter_SETXATTR,

   MetaOpCounter_MIRROR, // any mirrored message. lump them all together

   // For compatibility new fields MUST be added directly above _OpCounterLastEnum
   // NOTE: New types must also be added to MetaOpToStringMapping below.

   MetaOpCounter_OpCounterLastEnum // This must be the last element (to get num of valid elems)
};

/**
 * Per-client storage server operation counters.
 */
enum StorageOpCounterTypes
{
   // sum of all operations, simplifies search of nodes with most operations
   StorageOpCounter_OPSUM = 0,

   StorageOpCounter_ACK,
   StorageOpCounter_SETCHANNELDIRECT,

   StorageOpCounter_GETLOCALFILESIZE,
   StorageOpCounter_SETLOCALATTR,
   StorageOpCounter_STATSTORAGEPATH, // STATFS
   StorageOpCounter_TRUNCLOCALFILE,

   StorageOpCounter_CLOSELOCAL,
   StorageOpCounter_FSYNCLOCAL,
   StorageOpCounter_READOPS,
   StorageOpCounter_READBYTES,  // NOTE: Do NOT use directly for counting, but use READOPS

   StorageOpCounter_WRITEOPS,
   StorageOpCounter_WRITEBYTES, // NOTE: Do NOT use directly for counting, but use WRITEOPS

   StorageOpCounter_GENERICDEBUG,
   StorageOpCounter_HEARTBEAT,
   StorageOpCounter_REMOVENODE,

   StorageOpCounter_REQUESTSTORAGEDATA,

   StorageOpCounter_UNLINK,

   // For compatibility new fields MUST be added directly above _OpCounterLastEnum
   // NOTE: New types must also be added to StorageOpToStringMapping below.

   StorageOpCounter_OpCounterLastEnum // must be the last element (to get num of valid elems)
};


/**
 * Used by MetaOpToStringMapping, StorageOpToStringMapping & Co to convert OpCounter enum value
 * into descriptive strings names.
 */
class OpToStringMapping
{
   public:
      static std::string mapMetaOpNum(int opNum)
      {
         switch (opNum)
         {
            case MetaOpCounter_OPSUM: return "sum";
            case MetaOpCounter_ACK: return "ack";
            case MetaOpCounter_CLOSE: return "close";
            case MetaOpCounter_GETENTRYINFO: return "entInf";
            case MetaOpCounter_FINDOWNER: return "fndOwn";
            case MetaOpCounter_MKDIR: return "mkdir";
            case MetaOpCounter_MKFILE: return "create";
            case MetaOpCounter_READDIR: return "rddir";
            case MetaOpCounter_REFRESHENTRYINFO: return "refrEnt";
            case MetaOpCounter_REQUESTMETADATA: return "mdsInf";
            case MetaOpCounter_RMDIR: return "rmdir";
            case MetaOpCounter_RMLINK: return "rmLnk";
            case MetaOpCounter_MOVINGDIRINSERT: return "mvDirIns";
            case MetaOpCounter_MOVINGFILEINSERT: return "mvFiIns";
            case MetaOpCounter_OPEN: return "open";
            case MetaOpCounter_RENAME: return "ren";
            case MetaOpCounter_SETCHANNELDIRECT: return "sChDrct";
            case MetaOpCounter_SETATTR: return "sAttr";
            case MetaOpCounter_SETDIRPATTERN: return "sDirPat";
            case MetaOpCounter_STAT: return "stat";
            case MetaOpCounter_STATFS: return "statfs";
            case MetaOpCounter_TRUNCATE: return "trunc";
            case MetaOpCounter_SYMLINK: return "symlnk";
            case MetaOpCounter_UNLINK: return "unlnk";
            case MetaOpCounter_LOOKUPINTENT_SIMPLE: return "lookLI";
            case MetaOpCounter_LOOKUPINTENT_STAT: return "statLI";
            case MetaOpCounter_LOOKUPINTENT_REVALIDATE: return "revalLI";
            case MetaOpCounter_LOOKUPINTENT_OPEN: return "openLI";
            case MetaOpCounter_LOOKUPINTENT_CREATE: return "createLI";
            case MetaOpCounter_HARDLINK: return "hardlnk";
            case MetaOpCounter_FLOCKAPPEND: return "flckAp";
            case MetaOpCounter_FLOCKENTRY: return "flckEn";
            case MetaOpCounter_FLOCKRANGE: return "flckRg";
            case MetaOpCounter_UPDATEDIRPARENT: return "dirparent";
            case MetaOpCounter_LISTXATTR: return "listXA";
            case MetaOpCounter_GETXATTR: return "getXA";
            case MetaOpCounter_REMOVEXATTR: return "rmXA";
            case MetaOpCounter_SETXATTR: return "setXA";
            case MetaOpCounter_MIRROR: return "mirror";
         }
         return StringTk::intToStr(opNum);
      }
      static std::string mapStorageOpNum(int opNum)
      {
          switch (opNum)
         {
            case StorageOpCounter_OPSUM: return "sum";
            case StorageOpCounter_ACK: return "ack";
            case StorageOpCounter_SETCHANNELDIRECT: return "sChDrct";
            case StorageOpCounter_GETLOCALFILESIZE: return "getFSize";
            case StorageOpCounter_SETLOCALATTR: return "sAttr";
            case StorageOpCounter_STATSTORAGEPATH: return "statfs";
            case StorageOpCounter_TRUNCLOCALFILE: return "trunc";
            case StorageOpCounter_CLOSELOCAL: return "close";
            case StorageOpCounter_FSYNCLOCAL: return "fsync";
            case StorageOpCounter_READOPS: return "ops-rd";
            case StorageOpCounter_READBYTES: return "B-rd";
            case StorageOpCounter_WRITEOPS: return "ops-wr";
            case StorageOpCounter_WRITEBYTES: return "B-wr";
            case StorageOpCounter_GENERICDEBUG: return "gendbg";
            case StorageOpCounter_HEARTBEAT: return "hrtbeat";
            case StorageOpCounter_REMOVENODE: return "remNode";
            case StorageOpCounter_REQUESTSTORAGEDATA: return "storInf";
            case StorageOpCounter_UNLINK: return "unlnk";
         }
         return StringTk::intToStr(opNum);
      }
};

#endif /* OPCOUNTERTYPES_H_ */

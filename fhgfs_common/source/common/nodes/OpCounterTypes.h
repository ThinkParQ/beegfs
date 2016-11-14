/*
 * OpCounterTypes.h - enum definitions and enum to name mapping
 *
 */

#ifndef OPCOUNTERTYPES_H_
#define OPCOUNTERTYPES_H_

#include <map>
#include <string>

#include <common/toolkit/StringTk.h>

typedef std::map<int, std::string> OpNumToStringMap;
typedef OpNumToStringMap::iterator OpNumToStringMapIter;


/**
 * Per-client metadata server operation counters.
 */
enum MetaOpCounterTypes
{
   MetaOpCounter_OPSUM = 0, // sum of all ops, simplifies search of nodes with most operations

   MetaOpCounter_ACK,
   MetaOpCounter_CLOSE,
   MetaOpCounter_GETENTRYINFO,
   MetaOpCounter_GETNODEINFO,
   MetaOpCounter_FINDOWNER,
   MetaOpCounter_LINK,
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
   MetaOpCounter_MIRRORMETADATA,
   MetaOpCounter_HARDLINK,
   MetaOpCounter_FLOCKAPPEND,
   MetaOpCounter_FLOCKENTRY,
   MetaOpCounter_FLOCKRANGE,
   MetaOpCounter_UPDATEDIRPARENT,
   MetaOpCounter_LISTXATTR,
   MetaOpCounter_GETXATTR,
   MetaOpCounter_REMOVEXATTR,
   MetaOpCounter_SETXATTR,

   // For compatibility new fields MUST be added directly above _OpCounterLastEnum
   // NOTE: New types must also be added to admon/gui/common/enums/MetaOpsEnum.java
   //       and MetaOpToStringMapping below.

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
   StorageOpCounter_OPENLOCAL,
   StorageOpCounter_READOPS,
   StorageOpCounter_READBYTES,  // NOTE: Do NOT use directly for counting, but use READOPS

   StorageOpCounter_WRITEOPS,
   StorageOpCounter_WRITEBYTES, // NOTE: Do NOT use directly for counting, but use WRITEOPS

   StorageOpCounter_GENERICDEBUG,
   StorageOpCounter_HEARTBEAT,
   StorageOpCounter_REMOVENODE,

   StorageOpCounter_GETNODEINFO,
   StorageOpCounter_REQUESTSTORAGEDATA,

   StorageOpCounter_UNLINK,

   // For compatibility new fields MUST be added directly above _OpCounterLastEnum
   // NOTE: New types must also be added to admon/gui/common/enums/StorageOpsEnum.java
   //       and StorageOpToStringMapping below.

   StorageOpCounter_OpCounterLastEnum // must be the last element (to get num of valid elems)
};


/**
 * Used by MetaOpToStringMapping, StorageOpToStringMapping & Co to convert OpCounter enum value
 * into descriptive strings names.
 */
class OpToStringMapping
{
   protected:
      OpNumToStringMap opToStrMap;

   public:

      /**
       * Find opNum in the map and return the corresponding string. If opNum is not found, the op
       * might come from a newer fhgfs version, so we simply return the number as string then.
       */
      std::string opNumToStr(int opNum)
      {
         OpNumToStringMapIter iter = this->opToStrMap.find(opNum);
         if (iter == this->opToStrMap.end() )
            return StringTk::intToStr(opNum);

         return iter->second;
      }
};


/**
 * All per-client/user meta op-counter numbers with their corresponding descriptive name strings.
 */
class MetaOpToStringMapping : public OpToStringMapping
{
   public:
      /*
       * keep the values in sync with fhgfs_admon_gui/src/fhgfs_admon_gui/common/tools/EnumTk.java
       */
      MetaOpToStringMapping()
      {
         opToStrMap[MetaOpCounter_OPSUM]              =  "sum";

         opToStrMap[MetaOpCounter_ACK]                =  "ack";
         opToStrMap[MetaOpCounter_CLOSE]              =  "close";
         opToStrMap[MetaOpCounter_GETENTRYINFO]       =  "entInf";
         opToStrMap[MetaOpCounter_GETNODEINFO]        =  "nodeInf";
         opToStrMap[MetaOpCounter_FINDOWNER]          =  "fndOwn";
         opToStrMap[MetaOpCounter_LINK]               =  "lnk";
         opToStrMap[MetaOpCounter_MKDIR]              =  "mkdir";
         opToStrMap[MetaOpCounter_MKFILE]             =  "create";
         opToStrMap[MetaOpCounter_READDIR]            =  "rddir";
         opToStrMap[MetaOpCounter_REFRESHENTRYINFO]   =  "refrEnt";
         opToStrMap[MetaOpCounter_REQUESTMETADATA]    =  "mdsInf";
         opToStrMap[MetaOpCounter_RMDIR]              =  "rmdir";
         opToStrMap[MetaOpCounter_RMLINK]             =  "rmLnk";
         opToStrMap[MetaOpCounter_MOVINGDIRINSERT]    =  "mvDirIns";
         opToStrMap[MetaOpCounter_MOVINGFILEINSERT]   =  "mvFiIns";
         opToStrMap[MetaOpCounter_OPEN]               =  "open";
         opToStrMap[MetaOpCounter_RENAME]             =  "ren";
         opToStrMap[MetaOpCounter_SETCHANNELDIRECT]   =  "sChDrct";
         opToStrMap[MetaOpCounter_SETATTR]            =  "sAttr";
         opToStrMap[MetaOpCounter_SETDIRPATTERN]      =  "sDirPat";
         opToStrMap[MetaOpCounter_STAT]               =  "stat";
         opToStrMap[MetaOpCounter_STATFS]             =  "statfs";
         opToStrMap[MetaOpCounter_TRUNCATE]           =  "trunc";
         opToStrMap[MetaOpCounter_SYMLINK]            =  "symlnk";
         opToStrMap[MetaOpCounter_UNLINK]             =  "unlnk";
         opToStrMap[MetaOpCounter_LOOKUPINTENT_SIMPLE]     = "lookLI";
         opToStrMap[MetaOpCounter_LOOKUPINTENT_STAT]       = "statLI";
         opToStrMap[MetaOpCounter_LOOKUPINTENT_REVALIDATE] = "revalLI";
         opToStrMap[MetaOpCounter_LOOKUPINTENT_OPEN]       = "openLI";
         opToStrMap[MetaOpCounter_LOOKUPINTENT_CREATE]     = "createLI";
         opToStrMap[MetaOpCounter_MIRRORMETADATA]          = "mirrorMD";
         opToStrMap[MetaOpCounter_HARDLINK]                = "hardlnk";
         opToStrMap[MetaOpCounter_FLOCKAPPEND]             = "flckAp";
         opToStrMap[MetaOpCounter_FLOCKENTRY]              = "flckEn";
         opToStrMap[MetaOpCounter_FLOCKRANGE]              = "flckRg";
         opToStrMap[MetaOpCounter_UPDATEDIRPARENT]         = "dirparent";
         opToStrMap[MetaOpCounter_LISTXATTR]               = "listXA";
         opToStrMap[MetaOpCounter_GETXATTR]                = "getXA";
         opToStrMap[MetaOpCounter_REMOVEXATTR]             = "rmXA";
         opToStrMap[MetaOpCounter_SETXATTR]                = "setXA";
      }
};

/**
 * All per-client/user storage op-counter numbers with their corresponding descriptive name strings.
 */
class StorageOpToStringMapping : public OpToStringMapping
{
   public:
      /*
       * keep the values in sync with fhgfs_admon_gui/src/fhgfs_admon_gui/common/tools/EnumTk.java
       */
      StorageOpToStringMapping()
      {
         opToStrMap[StorageOpCounter_OPSUM]              =  "sum";
         opToStrMap[StorageOpCounter_ACK]                =  "ack";
         opToStrMap[StorageOpCounter_SETCHANNELDIRECT]   =  "sChDrct";
         opToStrMap[StorageOpCounter_GETLOCALFILESIZE]   =  "getFSize";
         opToStrMap[StorageOpCounter_SETLOCALATTR]       =  "sAttr";
         opToStrMap[StorageOpCounter_STATSTORAGEPATH]    =  "statfs";
         opToStrMap[StorageOpCounter_TRUNCLOCALFILE]     =  "trunc";
         opToStrMap[StorageOpCounter_CLOSELOCAL]         =  "close";
         opToStrMap[StorageOpCounter_FSYNCLOCAL]         =  "fsync";
         opToStrMap[StorageOpCounter_OPENLOCAL]          =  "open";
         opToStrMap[StorageOpCounter_READOPS]            =  "ops-rd";
         opToStrMap[StorageOpCounter_READBYTES]          =  "B-rd";
         opToStrMap[StorageOpCounter_WRITEOPS]           =  "ops-wr";
         opToStrMap[StorageOpCounter_WRITEBYTES]         =  "B-wr";
         opToStrMap[StorageOpCounter_GENERICDEBUG]       =  "gendbg";
         opToStrMap[StorageOpCounter_HEARTBEAT]          =  "hrtbeat";
         opToStrMap[StorageOpCounter_REMOVENODE]         =  "remNode";
         opToStrMap[StorageOpCounter_GETNODEINFO]        =  "nodeInf";
         opToStrMap[StorageOpCounter_REQUESTSTORAGEDATA] =  "storInf";
         opToStrMap[StorageOpCounter_UNLINK]             =  "unlnk";
      }
};


#endif /* OPCOUNTERTYPES_H_ */

/*
 * OpCounterTypes.h - enum definitions and enum to name mapping
 *
 */

#ifndef OPCOUNTERTYPES_H_
#define OPCOUNTERTYPES_H_

typedef std::map<int, std::string> OpNumToStringMap;
typedef OpNumToStringMap::iterator OpNumToStringMapIter;


enum MetaOpCounterTypes {
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

   // For compatibility new fields MUST be added directly above MetaOpCounter_OpCounterLastEnum

   // This MUST be the last element! Used by the iterator for initialization!
   MetaOpCounter_OpCounterLastEnum
};

enum StorageOpCounterTypes {
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
   StorageOpCounter_READBYTES,  // NOTE: Do NOT use directly for counting, but use READEOPS

   StorageOpCounter_WRITEOPS,
   StorageOpCounter_WRITEBYTES, // NOTE: Do NOT use directly for counting, but use WRITEOPS

   StorageOpCounter_GENERICDEBUG,
   StorageOpCounter_HEARTBEAT,
   StorageOpCounter_REMOVENODE,

   StorageOpCounter_GETNODEINFO,
   StorageOpCounter_REQUESTSTORAGEDATA,

   // For compatibility new fields MUST be added directly above StorageOpCounter_OpCounterLastEnum

   // This MUST be the last element! Used by the iterator for initialization!
   StorageOpCounter_OpCounterLastEnum
};

/**
 * Used by MetaOpMapping and StorageOpMapping to store convert OpCounter enum value into strings
 */
class OpToStringMapping
{
   protected:
      OpNumToStringMap OpToStrMap;

   public:

      /**
       * Find opNum in the map and return the corresponding string. If opNum is not found,
       * there is likely a bug somewhere, but we simply return the number as string then.
       */
      std::string OpNumToStr(int opNum)
      {
         OpNumToStringMapIter iter = this->OpToStrMap.find(opNum);
         if (iter == this->OpToStrMap.end() )
            return StringTk::intToStr(opNum);

         return iter->second;
      }
};


/**
 * Just a helper class to store all the meta Op-Counter numbers and their corresponding strings
 */
class MetaOpMapping : public OpToStringMapping
{
   public:

      MetaOpMapping()
      {
         OpToStrMap[MetaOpCounter_OPSUM]              =  "sum";
         OpToStrMap[MetaOpCounter_CLOSE]              =  "close";
         OpToStrMap[MetaOpCounter_ACK]                =  "ack";
         OpToStrMap[MetaOpCounter_GETENTRYINFO]       =  "eInfo";
         OpToStrMap[MetaOpCounter_GETNODEINFO]        =  "nInfo";
         OpToStrMap[MetaOpCounter_FINDOWNER]          =  "fOwner";
         OpToStrMap[MetaOpCounter_LINK]               =  "lnk";
         OpToStrMap[MetaOpCounter_MKDIR]              =  "mkdir";
         OpToStrMap[MetaOpCounter_MKFILE]             =  "create";
         OpToStrMap[MetaOpCounter_READDIR]            =  "rddir";
         OpToStrMap[MetaOpCounter_REFRESHENTRYINFO]   =  "refEInfo";
         OpToStrMap[MetaOpCounter_REQUESTMETADATA]    =  "reqMeDa";
         OpToStrMap[MetaOpCounter_RMDIR]              =  "rmdir";
         OpToStrMap[MetaOpCounter_RMLINK]             =  "rmLnk";
         OpToStrMap[MetaOpCounter_MOVINGDIRINSERT]    =  "mvDirIns";
         OpToStrMap[MetaOpCounter_MOVINGFILEINSERT]   =  "mvFiIns";
         OpToStrMap[MetaOpCounter_OPEN]               =  "open";
         OpToStrMap[MetaOpCounter_RENAME]             =  "ren";
         OpToStrMap[MetaOpCounter_SETCHANNELDIRECT]   =  "setChDir";
         OpToStrMap[MetaOpCounter_SETATTR]            =  "sAttr";
         OpToStrMap[MetaOpCounter_SETDIRPATTERN]      =  "sDirPat";
         OpToStrMap[MetaOpCounter_STAT]               =  "stat";
         OpToStrMap[MetaOpCounter_STATFS]             =  "statfs";
         OpToStrMap[MetaOpCounter_TRUNCATE]           =  "trunc";
         OpToStrMap[MetaOpCounter_SYMLINK]            =  "sylnk";
         OpToStrMap[MetaOpCounter_UNLINK]             =  "unlnk";
         OpToStrMap[MetaOpCounter_LOOKUPINTENT_SIMPLE]     = "lookLI";
         OpToStrMap[MetaOpCounter_LOOKUPINTENT_STAT]       = "statLI";
         OpToStrMap[MetaOpCounter_LOOKUPINTENT_REVALIDATE] = "revalLI";
         OpToStrMap[MetaOpCounter_LOOKUPINTENT_OPEN]       = "openLI";
         OpToStrMap[MetaOpCounter_LOOKUPINTENT_CREATE]     = "createLI";
      }
};

/**
 * Just a helper class to store all the storage Op-Counter numbers and their corresponding strings
 */
class StorageOpMapping : public OpToStringMapping
{
   public:
      StorageOpMapping()
      {
         OpToStrMap[StorageOpCounter_OPSUM]              =  "sum";
         OpToStrMap[StorageOpCounter_ACK]                =  "ack";
         OpToStrMap[StorageOpCounter_SETCHANNELDIRECT]   =  "setChDir";
         OpToStrMap[StorageOpCounter_GETLOCALFILESIZE]   =  "getFSize";
         OpToStrMap[StorageOpCounter_SETLOCALATTR]       =  "setAttr";
         OpToStrMap[StorageOpCounter_STATSTORAGEPATH]    =  "statfs";
         OpToStrMap[StorageOpCounter_TRUNCLOCALFILE]     =  "trunc";
         OpToStrMap[StorageOpCounter_CLOSELOCAL]         =  "close";
         OpToStrMap[StorageOpCounter_FSYNCLOCAL]         =  "fsync";
         OpToStrMap[StorageOpCounter_OPENLOCAL]          =  "open";
         OpToStrMap[StorageOpCounter_READOPS]            =  "ops-rd";
         OpToStrMap[StorageOpCounter_READBYTES]          =  "B-rd";
         OpToStrMap[StorageOpCounter_WRITEOPS]           =  "ops-wr";
         OpToStrMap[StorageOpCounter_WRITEBYTES]         =  "B-wr";
         OpToStrMap[StorageOpCounter_GENERICDEBUG]       =  "gdebug";
         OpToStrMap[StorageOpCounter_HEARTBEAT]          =  "hb";
         OpToStrMap[StorageOpCounter_REMOVENODE]         =  "remNode";
         OpToStrMap[StorageOpCounter_GETNODEINFO]        =  "nodeInfo";
         OpToStrMap[StorageOpCounter_REQUESTSTORAGEDATA] =  "reqStorDat";
      }
};



#endif /* OPCOUNTERTYPES_H_ */

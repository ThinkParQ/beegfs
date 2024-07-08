#ifndef _BEEGFS_CLIENT_H_INCLUDED
#define _BEEGFS_CLIENT_H_INCLUDED

#define BEEGFS_IOCTL_CFG_MAX_PATH 4096 // just an arbitrary value, has to be identical in user space
#define BEEGFS_IOCTL_TEST_STRING  "_FhGFS_" /* copied to user space by BEEGFS_IOC_TEST_IS_FHGFS to
                                 to confirm an FhGFS mount */
#define BEEGFS_IOCTL_TEST_BUFLEN  6 /* note: char[6] is actually the wrong size for the
                                 BEEGFS_IOCTL_TEST_STRING that is exchanged, but that is no problem
                                 in this particular case and so we keep it for compatibility */
#define BEEGFS_IOCTL_MOUNTID_BUFLEN     256
#define BEEGFS_IOCTL_NODESTRID_BUFLEN   256
#define BEEGFS_IOCTL_NODETYPE_BUFLEN    16
#define BEEGFS_IOCTL_FILENAME_MAXLEN    256 // max supported filename len (incl terminating zero)

// entryID string is made of three 32 bit values in hexadecimal form plus two dashes
// (see common/toolkit/StorageTk.h)
#define BEEGFS_IOCTL_ENTRYID_MAXLEN     26

// stripe pattern types
#define BEEGFS_STRIPEPATTERN_INVALID      0
#define BEEGFS_STRIPEPATTERN_RAID0        1
#define BEEGFS_STRIPEPATTERN_RAID10       2
#define BEEGFS_STRIPEPATTERN_BUDDYMIRROR  3

#define BEEGFS_IOCTL_PING_MAX_COUNT       10
#define BEEGFS_IOCTL_PING_MAX_INTERVAL    2000
#define BEEGFS_IOCTL_PING_NODE_BUFLEN     64
#define BEEGFS_IOCTL_PING_SOCKTYPE_BUFLEN 8

/*
 * General notes:
 * - the _IOR() macro is for ioctls that read information, _IOW refers to ioctls that write or make
 *    modifications (e.g. file creation).
 *
 * - _IOR(type, number, data_type) meanings:
 *    - note: _IOR() encodes all three values (type, number, data_type size) into the request number
 *    - type: 8 bit driver-specific number to identify the driver if there are multiple drivers
 *       listening to the same fd (e.g. such as the TCP and IP layers).
 *    - number: 8 bit integer command number, so different numbers for different routines.
 *    - data_type: the data type (size) to be exchanged with the driver (though this number can
 *       also rather be seen as a command number subversion, because the actual number given here is
 *       not really exchanged unless the drivers' ioctl handler explicity does the exchange).
 */

#define BEEGFS_IOCTYPE_ID                     'f'

#define BEEGFS_IOCNUM_GETVERSION_OLD           1 // value from FS_IOC_GETVERSION in linux/fs.h
#define BEEGFS_IOCNUM_GETVERSION               3
#define BEEGFS_IOCNUM_GET_CFG_FILE            20
#define BEEGFS_IOCNUM_CREATE_FILE             21
#define BEEGFS_IOCNUM_TEST_IS_FHGFS           22
#define BEEGFS_IOCNUM_TEST_IS_BEEGFS          22
#define BEEGFS_IOCNUM_GET_RUNTIME_CFG_FILE    23
#define BEEGFS_IOCNUM_GET_MOUNTID             24
#define BEEGFS_IOCNUM_GET_STRIPEINFO          25
#define BEEGFS_IOCNUM_GET_STRIPETARGET        26
#define BEEGFS_IOCNUM_MKFILE_STRIPEHINTS      27
#define BEEGFS_IOCNUM_CREATE_FILE_V2          28
#define BEEGFS_IOCNUM_CREATE_FILE_V3          29
#define BEEGFS_IOCNUM_GETINODEID              30
#define BEEGFS_IOCNUM_GETENTRYINFO            31
#define BEEGFS_IOCNUM_PINGNODE                32

#define BEEGFS_IOC_GETVERSION     _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETVERSION, long)
#define BEEGFS_IOC32_GETVERSION   _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETVERSION, int)
#define BEEGFS_IOC_GET_CFG_FILE   _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_CFG_FILE, struct BeegfsIoctl_GetCfgFile_Arg)
#define BEEGFS_IOC_CREATE_FILE    _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_CREATE_FILE, struct BeegfsIoctl_MkFile_Arg)
#define BEEGFS_IOC_CREATE_FILE_V2  _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_CREATE_FILE_V2, struct BeegfsIoctl_MkFileV2_Arg)
#define BEEGFS_IOC_CREATE_FILE_V3  _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_CREATE_FILE_V3, struct BeegfsIoctl_MkFileV3_Arg)
#define BEEGFS_IOC_TEST_IS_FHGFS  _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_TEST_IS_FHGFS, char[BEEGFS_IOCTL_TEST_BUFLEN])
#define BEEGFS_IOC_TEST_IS_BEEGFS  _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_TEST_IS_BEEGFS, char[BEEGFS_IOCTL_TEST_BUFLEN])
#define BEEGFS_IOC_GET_RUNTIME_CFG_FILE    _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_RUNTIME_CFG_FILE, struct BeegfsIoctl_GetCfgFile_Arg)
#define BEEGFS_IOC_GET_MOUNTID    _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_MOUNTID, char[BEEGFS_IOCTL_MOUNTID_BUFLEN])
#define BEEGFS_IOC_GET_STRIPEINFO          _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_STRIPEINFO, struct BeegfsIoctl_GetStripeInfo_Arg)
#define BEEGFS_IOC_GET_STRIPETARGET        _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_STRIPETARGET, struct BeegfsIoctl_GetStripeTarget_Arg)
#define BEEGFS_IOC_GET_STRIPETARGET_V2     _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_STRIPETARGET, struct BeegfsIoctl_GetStripeTargetV2_Arg)
#define BEEGFS_IOC_MKFILE_STRIPEHINTS      _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_MKFILE_STRIPEHINTS, struct BeegfsIoctl_MkFileWithStripeHints_Arg)
#define BEEGFS_IOC_GETINODEID             _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETINODEID, struct BeegfsIoctl_GetInodeID_Arg)
#define BEEGFS_IOC_GETENTRYINFO             _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETENTRYINFO, struct BeegfsIoctl_GetEntryInfo_Arg)
#define BEEGFS_IOC_PINGNODE                 _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_PINGNODE, struct BeegfsIoctl_PingNode_Arg)


/* used to return the client config file path using an IOCTL */
struct BeegfsIoctl_GetCfgFile_Arg
{
   char path[BEEGFS_IOCTL_CFG_MAX_PATH]; // (out-value) where the result path will be stored
   int length;                          /* (in-value) length of path buffer (unused, because it's
                                           after a fixed-size path buffer anyways) */
};

/* used to pass information for file creation */
struct BeegfsIoctl_MkFile_Arg
{
   uint16_t ownerNodeID; // owner node of the parent dir

   const char* parentParentEntryID; // entryID of the parent of the parent (=> the grandparentID)
   int parentParentEntryIDLen;

   const char* parentEntryID; // entryID of the parent
   int parentEntryIDLen;

   const char* parentName;   // name of parent dir
   int parentNameLen;

   // file information
   const char* entryName; // file name we want to create
   int entryNameLen;
   int fileType; // see linux/fs.h or man 3 readdir, DT_UNKNOWN, DT_FIFO, ...

   const char* symlinkTo;  // Only must be set for symlinks. The name a symlink is supposed to point to
   int symlinkToLen; // Length of the symlink name

   int mode; // mode (permission) of the new file

   // user ID and group only will be used, if the current user is root
   uid_t uid; // user ID
   gid_t gid; // group ID

   int numTargets;     // number of targets in prefTargets array (without final 0 element)
   uint16_t* prefTargets;  // array of preferred targets (additional last element must be 0)
   int prefTargetsLen; // raw byte length of prefTargets array (including final 0 element)
};

struct BeegfsIoctl_MkFileV2_Arg
{
   uint32_t ownerNodeID; // owner node/group of the parent dir

   const char* parentParentEntryID; // entryID of the parent of the parent (=> the grandparentID)
   int parentParentEntryIDLen;

   const char* parentEntryID; // entryID of the parent
   int parentEntryIDLen;
   int parentIsBuddyMirrored;

   const char* parentName;   // name of parent dir
   int parentNameLen;

   // file information
   const char* entryName; // file name we want to create
   int entryNameLen;
   int fileType; // see linux/fs.h or man 3 readdir, DT_UNKNOWN, DT_FIFO, ...

   char* symlinkTo;  // Only must be set for symlinks. The name a symlink is supposed to point to
   int symlinkToLen; // Length of the symlink name

   int mode; // mode (permission) of the new file

   // user ID and group only will be used, if the current user is root
   uid_t uid; // user ID
   gid_t gid; // group ID

   int numTargets;     // number of targets in prefTargets array (without final 0 element)
   uint16_t* prefTargets;  // array of preferred targets (additional last element must be 0)
   int prefTargetsLen; // raw byte length of prefTargets array (including final 0 element)
};

struct BeegfsIoctl_MkFileV3_Arg
{
   uint32_t ownerNodeID; // owner node/group of the parent dir

   const char* parentParentEntryID; // entryID of the parent of the parent (=> the grandparentID)
   int parentParentEntryIDLen;

   const char* parentEntryID; // entryID of the parent
   int parentEntryIDLen;
   int parentIsBuddyMirrored;

   const char* parentName;   // name of parent dir
   int parentNameLen;

   // file information
   const char* entryName; // file name we want to create
   int entryNameLen;
   int fileType; // see linux/fs.h or man 3 readdir, DT_UNKNOWN, DT_FIFO, ...

   const char* symlinkTo;  // Only must be set for symlinks. The name a symlink is supposed to point to
   int symlinkToLen; // Length of the symlink name

   int mode; // mode (permission) of the new file

   // user ID and group only will be used, if the current user is root
   uid_t uid; // user ID
   gid_t gid; // group ID

   int numTargets;     // number of targets in prefTargets array (without final 0 element)
   uint16_t* prefTargets;  // array of preferred targets (additional last element must be 0)
   int prefTargetsLen; // raw byte length of prefTargets array (including final 0 element)

   uint16_t storagePoolId; // if set, this is used to override the pool id of the parent dir
};

/* used to get the stripe info of a file */
struct BeegfsIoctl_GetStripeInfo_Arg
{
   unsigned outPatternType; // (out-value) stripe pattern type (STRIPEPATTERN_...)
   unsigned outChunkSize; // (out-value) chunksize for striping
   uint16_t outNumTargets; // (out-value) number of stripe targets of given file
};

/* used to get the stripe target of a file */
struct BeegfsIoctl_GetStripeTarget_Arg
{
   uint16_t targetIndex; // index of the target that should be queried (0-based)

   uint16_t outTargetNumID; // (out-value) numeric ID of target with given index
   uint16_t outNodeNumID;   // (out-value) numeric ID of node to which this target is mapped
   char outNodeStrID[BEEGFS_IOCTL_NODESTRID_BUFLEN]; /* (out-value) string ID of node to which this
                                                       target is mapped */
};

struct BeegfsIoctl_GetStripeTargetV2_Arg
{
   /* inputs */
   uint32_t targetIndex;

   /* outputs */
   uint32_t targetOrGroup; // target ID if the file is not buddy mirrored, otherwise mirror group ID

   uint32_t primaryTarget; // target ID != 0 if buddy mirrored
   uint32_t secondaryTarget; // target ID != 0 if buddy mirrored

   uint32_t primaryNodeID; // node ID of target (if unmirrored) or primary target (if mirrored)
   uint32_t secondaryNodeID; // node ID of secondary target, or 0 if unmirrored

   char primaryNodeStrID[BEEGFS_IOCTL_NODESTRID_BUFLEN];
   char secondaryNodeStrID[BEEGFS_IOCTL_NODESTRID_BUFLEN];
};

/* used to pass information for file creation with stripe hints */
struct BeegfsIoctl_MkFileWithStripeHints_Arg
{
   const char* filename; // file name we want to create
   unsigned mode; // mode (access permission) of the new file

   unsigned numtargets; // number of desired targets, 0 for directory default
   unsigned chunksize; // in bytes, must be 2^n >= 64Ki, 0 for directory default
};

struct BeegfsIoctl_GetInodeID_Arg
{
   // input
   char entryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];

   // output
   uint64_t inodeID;

};

struct BeegfsIoctl_GetEntryInfo_Arg
{
   uint32_t ownerID;
   char parentEntryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];
   char entryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];
   int entryType;
   int featureFlags;
};

struct BeegfsIoctl_PingNode_Arg_Params
{
   uint32_t nodeId;
   char nodeType[BEEGFS_IOCTL_NODETYPE_BUFLEN];
   unsigned count;
   unsigned interval;
};

struct BeegfsIoctl_PingNode_Arg_Results
{
   char outNode[BEEGFS_IOCTL_PING_NODE_BUFLEN];
   unsigned outSuccess;
   unsigned outErrors;
   unsigned outTotalTime;
   unsigned outPingTime[BEEGFS_IOCTL_PING_MAX_COUNT];
   char outPingType[BEEGFS_IOCTL_PING_MAX_COUNT][BEEGFS_IOCTL_PING_SOCKTYPE_BUFLEN];
};

struct BeegfsIoctl_PingNode_Arg
{
   struct BeegfsIoctl_PingNode_Arg_Params params;
   struct BeegfsIoctl_PingNode_Arg_Results results;
};


#endif

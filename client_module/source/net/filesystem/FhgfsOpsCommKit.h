#ifndef FHGFSOPSCOMMKIT_H_
#define FHGFSOPSCOMMKIT_H_

#include <common/storage/PathInfo.h>
#include <os/iov_iter.h>
#include "FhgfsOpsCommKitCommon.h"

struct ReadfileState;
typedef struct ReadfileState ReadfileState;
struct WritefileState;
typedef struct WritefileState WritefileState;


struct FsyncContext;


bool FhgfsOpsCommKit_initEmergencyPools(void);
void FhgfsOpsCommKit_releaseEmergencyPools(void);


extern void FhgfsOpsCommkit_communicate(App* app, RemotingIOInfo* ioInfo,
   struct list_head* targetInfos, const struct CommKitContextOps* ops, void* private);


extern void FhgfsOpsCommKit_readfileV2bCommunicate(App* app, RemotingIOInfo* ioInfo,
   struct list_head* states, void (*nextIter)(CommKitContext*, ReadfileState*),
   void (*prepare)(CommKitContext*, ReadfileState*));

extern void FhgfsOpsCommKit_writefileV2bCommunicate(App* app, RemotingIOInfo* ioInfo,
   struct list_head* states, void (*nextIter)(CommKitContext*, WritefileState*),
   void (*prepare)(CommKitContext*, WritefileState*));

extern void FhgfsOpsCommKit_fsyncCommunicate(App* app, RemotingIOInfo* ioInfo,
   struct FsyncContext* context);

extern void FhgfsOpsCommKit_statStorageCommunicate(App* app, struct list_head* targets);



extern void FhgfsOpsCommKit_initReadfileState(ReadfileState* state, loff_t offset, size_t size,
   uint16_t targetID);
extern void FhgfsOpsCommKit_initWritefileState(WritefileState* state, loff_t offset, size_t size,
   uint16_t targetID);


enum CommKitState
{
   CommKitState_PREPARE,
   CommKitState_SENDHEADER,
   CommKitState_SENDDATA,
   CommKitState_RECVHEADER,
   CommKitState_RECVDATA,
   CommKitState_SOCKETINVALIDATE,
   CommKitState_CLEANUP,
   CommKitState_RETRYWAIT,
   CommKitState_DONE,
};

struct CommKitTargetInfo
{
   struct list_head targetInfoList;

   // (set to _PREPARE in the beginning, assigned by the state-creator)
   enum CommKitState state; // the current stage of the individual communication process

   // used by GenericResponse handler
   struct {
      unsigned peerTryAgain:1;
      unsigned indirectCommError:1;
      unsigned indirectCommErrorNoRetry:1;
   } logged;

   // assigned by the state-creator
   char* headerBuffer; // for serialization
   uint16_t targetID;
   uint16_t selectedTargetID; // either targetID or the buddy, if useBuddyMirrorSecond
   bool useBuddyMirrorSecond; // if buddy mirroring, this msg goes to secondary

   // set by _PREPARE handler
   Node* node; // target-node reference
   Socket* socket; // target-node connection
   unsigned headerSize;

   // error if negative, other set by specialized actions
   int64_t nodeResult;
};



struct ReadfileState
{
   struct CommKitTargetInfo base;

   // data for spefic modes (will be assigned by the corresponding modes)
   size_t transmitted; // how much data has been transmitted already
   size_t toBeTransmitted; // how much data has to be transmitted
   size_t totalReadSize; // how much data was requested

   // data for all modes

   // (assigned by the state-creator)
   struct iov_iter data;
   loff_t offset; // target-node local offset

   bool firstWriteDoneForTarget; /* true if a chunk was previously written to this target in
                                          this session; used for the session check */
   bool receiveFileData; /* if false, receive the int64 fragment length, else the fragment */

   // result data
   int64_t expectedNodeResult; // the amount of data that we wanted to read
};


struct WritefileState
{
   struct CommKitTargetInfo base;

   // data for specific stages (will be assigned by the corresponding stages)
   size_t transmitted; // how much data has been transmitted already
   size_t toBeTransmitted; // how much data has to be transmitted
   size_t totalWriteSize; // how much data should be sent

   // data for all stages

   // (assigned by the state-creator)
   struct iov_iter data;
   loff_t offset; // target-node local offset

   bool firstWriteDoneForTarget; /* true if data was previously written to this target in
                                          this session; used for the session check */

   // result data
   int64_t expectedNodeResult; // the amount of data that we wanted to write
};


struct FsyncContext {
   unsigned userID;
   bool forceRemoteFlush;
   bool checkSession;
   bool doSyncOnClose;

   struct list_head states;
};

struct FsyncState {
   struct CommKitTargetInfo base;

   bool firstWriteDoneForTarget;
};

extern void FhgfsOpsCommKit_initFsyncState(struct FsyncContext* context, struct FsyncState* state,
   uint16_t targetID);



struct StatStorageState
{
   struct CommKitTargetInfo base;

   int64_t totalSize;
   int64_t totalFree;
};

extern void FhgfsOpsCommKit_initStatStorageState(struct list_head* states,
   struct StatStorageState* state, uint16_t targetID);

#endif /*FHGFSOPSCOMMKIT_H_*/

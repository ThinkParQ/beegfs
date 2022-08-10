#ifndef FHGFSOPSCOMMKITVEC_H_
#define FHGFSOPSCOMMKITVEC_H_

#include <net/filesystem/FhgfsOpsRemoting.h>
#include <toolkit/FhgfsPage.h>
#include <toolkit/FhgfsChunkPageVec.h>

#include <linux/fs.h>

#include "FhgfsOpsCommKitCommon.h"
#include "RemotingIOInfo.h"


struct FhgfsCommKitVec;
typedef struct FhgfsCommKitVec FhgfsCommKitVec;

struct CommKitVecHelper;
typedef struct CommKitVecHelper CommKitVecHelper;


extern int64_t FhgfsOpsCommKitVec_rwFileCommunicate(App* app, RemotingIOInfo* ioInfo,
   FhgfsCommKitVec* comm, Fhgfs_RWType rwType);


static inline FhgfsCommKitVec FhgfsOpsCommKitVec_assignRWfileState(FhgfsChunkPageVec* pageVec,
   unsigned pageIdx, unsigned numPages, loff_t offset, uint16_t targetID, char* msgBuf);

static inline void FhgfsOpsCommKitVec_setRWFileStateFirstWriteDone(
   bool firstWriteDoneForTarget, FhgfsCommKitVec* outComm);

static inline size_t FhgfsOpsCommKitVec_getRemainingDataSize(FhgfsCommKitVec* comm);
static inline loff_t FhgfsOpsCommKitVec_getOffset(FhgfsCommKitVec* comm);



struct FhgfsCommKitVec
{
   // Depending if we are going to read or write, the corresponding struct will be used
   union
   {
         // NOTE: The *initial* stage for read and write MUST be RW_FILE_STAGE_PREPARE
         struct readState
         {
               size_t reqLen;     // requested size from client to server
               size_t serverSize; // how many data the server is going to send (<= reqLen)
         } read ;

         struct writeState
         {
               WriteLocalFileRespMsg respMsg; // response message
         } write;
   };

   size_t hdrLen; // size (length) of the header message

   size_t numSuccessPages; // number of pages successfully handled

   // data for all stages

   // (assigned by the state-creator)
   FhgfsChunkPageVec* pageVec;
   loff_t initialOffset; // initial offset before sending any data

   uint16_t targetID; // target ID
   char* msgBuf; // for serialization

   bool firstWriteDoneForTarget; /* true if data was previously written to this target in
                                          this session; used for the session check */
   bool useBuddyMirrorSecond; // for buddy mirroring, this msg goes to secondary
   bool useServersideMirroring; // data shall be forwarded to mirror by primary

   // (assigned by the preparation stage)
   Node* node; // target-node reference
   Socket* sock; // target-node connection

   // result information
   int64_t nodeResult; /* the amount of data that have been read/written on the server side
                        * (or a negative fhgfs error code) */

   bool doHeader; // continue with the read/write header stage
};

/**
 * Additional data that is required or useful for all the states.
 * (This is shared states data.)
 *
 * invariant: (numRetryWaiters + isDone + numUnconnectable + numPollSocks) <= numStates
 */
struct CommKitVecHelper
{
   App* app;
   Logger* log;
   char* logContext;

   RemotingIOInfo* ioInfo;
};


/**
 * Note: Caller still MUST initialize type.{read/write}.stage = RW_FILE_STAGE_PREPARE.
 * Note: Initializes the expectedNodeResult attribute from the size argument.
 * Note: Defaults to server-side mirroring.
 */
FhgfsCommKitVec FhgfsOpsCommKitVec_assignRWfileState(FhgfsChunkPageVec* pageVec,
   unsigned pageIdx, unsigned numPages, loff_t offset, uint16_t targetID, char* msgBuf)
{
   FhgfsCommKitVec state =
   {
      .initialOffset      = offset,
      .targetID           = targetID,
      .msgBuf             = msgBuf,

      .firstWriteDoneForTarget = false, // set via separate setter method
      .useBuddyMirrorSecond    = false, // set via separate setter method
      .useServersideMirroring  = true, // set via separate setter method

      .nodeResult         = -FhgfsOpsErr_INTERNAL,
      .numSuccessPages    = 0,
   };

   state.pageVec = pageVec;

   return state;
}

void FhgfsOpsCommKitVec_setRWFileStateFirstWriteDone(bool firstWriteDoneForTarget,
   FhgfsCommKitVec* outComm)
{
   outComm->firstWriteDoneForTarget = firstWriteDoneForTarget;
}


/**
 * Get the overall remaining data size (chunkPageVec + last incompletely processed page)
 */
size_t FhgfsOpsCommKitVec_getRemainingDataSize(FhgfsCommKitVec* comm)
{
   return FhgfsChunkPageVec_getRemainingDataSize(comm->pageVec);
}

/**
 * Get the current offset
 */
loff_t FhgfsOpsCommKitVec_getOffset(FhgfsCommKitVec* comm)
{
   FhgfsChunkPageVec* pageVec = comm->pageVec;

   // data size already send/received (written/read)
   size_t processedSize =
      FhgfsChunkPageVec_getDataSize(pageVec) - FhgfsOpsCommKitVec_getRemainingDataSize(comm);

   return comm->initialOffset + processedSize;
}


#endif /*FHGFSOPSCOMMKITVEC_H_*/

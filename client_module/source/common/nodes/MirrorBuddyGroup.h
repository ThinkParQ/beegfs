#ifndef MIRRORBUDDYGROUP_H
#define MIRRORBUDDYGROUP_H

#include <common/Common.h>
#include <common/toolkit/MathTk.h>
#include <os/OsCompat.h>

struct MirrorBuddyGroup;
typedef struct MirrorBuddyGroup MirrorBuddyGroup;

// used to build a binary min-heap with backlink handles.
// an inserter will receive a handle to the inserted value, this handle may be used to remove the
// value in logarithmic time.
struct BuddySequenceNumber
{
   struct BuddySequenceNumber** pSelf;
   uint64_t value;
};

struct MirrorBuddyGroup
{
   uint16_t groupID;

   uint16_t firstTargetID;
   uint16_t secondTargetID;

   uint64_t sequence;

   struct semaphore slotsAvail;

   struct BuddySequenceNumber* seqNoInFlight;
   unsigned inFlightSize;
   unsigned inFlightCapacity;

   uint64_t* finishedSeqNums;
   unsigned firstFinishedIndex;
   unsigned finishedCount;

   struct mutex mtx;

   struct kref refs;

/* private */
   union {
      struct rb_node _rb_node;
      struct list_head _list;
   };
};

extern struct MirrorBuddyGroup* MirrorBuddyGroup_constructFromTargetIDs(uint16_t groupID,
      uint16_t doneBufferSize, uint16_t firstTargetID, uint16_t secondTargetID);
extern void MirrorBuddyGroup_put(MirrorBuddyGroup* this);

extern int MirrorBuddyGroup_acquireSequenceNumber(MirrorBuddyGroup* this,
   uint64_t* acknowledgeSeq, bool* isSelective, uint64_t* seqNo,
   struct BuddySequenceNumber** handle, bool allowWait);
extern void MirrorBuddyGroup_releaseSequenceNumber(MirrorBuddyGroup* this,
   struct BuddySequenceNumber** handle);

extern void MirrorBuddyGroup_setSeqNoBase(MirrorBuddyGroup* this, uint64_t seqNoBase);

#endif /* MIRRORBUDDYGROUP_H */

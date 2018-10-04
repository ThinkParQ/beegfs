#include "MirrorBuddyGroup.h"

struct MirrorBuddyGroup* MirrorBuddyGroup_constructFromTargetIDs(uint16_t groupID,
      uint16_t doneBufferSize, uint16_t firstTargetID, uint16_t secondTargetID)
{
   struct MirrorBuddyGroup* this = (MirrorBuddyGroup*) os_kmalloc(sizeof(*this));

   if (!this)
      return NULL;

   this->groupID = groupID;

   this->firstTargetID = firstTargetID;
   this->secondTargetID = secondTargetID;
   this->sequence = 0;

   if (doneBufferSize == 0)
      goto fail;

   this->inFlightSize = 0;
   this->inFlightCapacity = doneBufferSize;
   this->seqNoInFlight = kmalloc(doneBufferSize * sizeof(struct BuddySequenceNumber), GFP_NOFS);
   if (!this->seqNoInFlight)
      this->seqNoInFlight = vmalloc(doneBufferSize * sizeof(struct BuddySequenceNumber));

   if (!this->seqNoInFlight)
      goto fail;

   this->firstFinishedIndex = 0;
   this->finishedCount = 0;
   this->finishedSeqNums = kmalloc(doneBufferSize * sizeof(uint64_t), GFP_NOFS);
   if (!this->finishedSeqNums)
      this->finishedSeqNums = vmalloc(doneBufferSize * sizeof(uint64_t));

   if (!this->finishedSeqNums)
      goto fail_seqNoInFlight;

   mutex_init(&this->mtx);
   sema_init(&this->slotsAvail, doneBufferSize);
   kref_init(&this->refs);

   return this;

fail_seqNoInFlight:
   if (is_vmalloc_addr(this->seqNoInFlight))
      vfree(this->seqNoInFlight);
   else
      kfree(this->seqNoInFlight);

fail:
   kfree(this);
   return NULL;
}

static void __MirrorBuddyGroup_destruct(struct kref* ref)
{
   MirrorBuddyGroup* this = container_of(ref, MirrorBuddyGroup, refs);

   if (is_vmalloc_addr(this->seqNoInFlight))
      vfree(this->seqNoInFlight);
   else
      kfree(this->seqNoInFlight);

   if (is_vmalloc_addr(this->finishedSeqNums))
      vfree(this->finishedSeqNums);
   else
      kfree(this->finishedSeqNums);

   mutex_destroy(&this->mtx);

   kfree(this);
}

void MirrorBuddyGroup_put(MirrorBuddyGroup* this)
{
   kref_put(&this->refs, __MirrorBuddyGroup_destruct);
}

int MirrorBuddyGroup_acquireSequenceNumber(MirrorBuddyGroup* this,
   uint64_t* acknowledgeSeq, bool* isSelective, uint64_t* seqNo,
   struct BuddySequenceNumber** handle, bool allowWait)
{
   int result = 0;

   if (allowWait)
   {
      if (down_killable(&this->slotsAvail))
         return EINTR;
   }
   else
   {
      if (down_trylock(&this->slotsAvail))
         return EAGAIN;
   }

   kref_get(&this->refs);

   mutex_lock(&this->mtx);
   do {
      if (this->sequence == 0)
      {
         *seqNo = 0;
         *handle = NULL;
         result = ENOENT;
         up(&this->slotsAvail);
         MirrorBuddyGroup_put(this);
         break;
      }

      *seqNo = ++this->sequence;

      // seqNoInFlight is a binary min-heap, and we add values from a strictly increasing sequence.
      // thus any such append produces a correctly formed heap.
      this->seqNoInFlight[this->inFlightSize].pSelf = handle;
      this->seqNoInFlight[this->inFlightSize].value = *seqNo;
      *handle = &this->seqNoInFlight[this->inFlightSize];

      this->inFlightSize++;

      if (this->finishedCount > 0)
      {
         *acknowledgeSeq = this->finishedSeqNums[this->firstFinishedIndex];
         this->firstFinishedIndex = (this->firstFinishedIndex + 1) % this->inFlightCapacity;
         this->finishedCount -= 1;
         *isSelective = true;
      }
      else
      {
         *acknowledgeSeq = this->seqNoInFlight[0].value - 1;
         *isSelective = false;
      }
   } while (0);
   mutex_unlock(&this->mtx);

   return result;
}

void MirrorBuddyGroup_releaseSequenceNumber(MirrorBuddyGroup* this,
   struct BuddySequenceNumber** handle)
{
   mutex_lock(&this->mtx);
   {
      struct BuddySequenceNumber* slot = *handle;
      unsigned nextFinishedIndex;

      BEEGFS_BUG_ON_DEBUG(slot < &this->seqNoInFlight[0], "");
      BEEGFS_BUG_ON_DEBUG(slot >= &this->seqNoInFlight[this->inFlightSize], "");

      // before maintaining the heap, add the sequence number to the "finished seq#" ringbuffer.
      if (this->finishedCount < this->inFlightCapacity)
      {
         this->finishedCount = this->finishedCount + 1;
         nextFinishedIndex = (this->firstFinishedIndex + this->finishedCount - 1)
            % this->inFlightCapacity;
      }
      else
      {
         this->firstFinishedIndex = (this->firstFinishedIndex + 1) % this->inFlightCapacity;
         nextFinishedIndex = this->firstFinishedIndex;
      }

      this->finishedSeqNums[nextFinishedIndex] = (*handle)->value;

      // decrease the key of the seq# to minimum
      while (slot != &this->seqNoInFlight[0])
      {
         const ptrdiff_t index = slot - &this->seqNoInFlight[0];
         const ptrdiff_t parentIndex = index / 2;

         swap(this->seqNoInFlight[index], this->seqNoInFlight[parentIndex]);

         *this->seqNoInFlight[index].pSelf = &this->seqNoInFlight[index];
         *this->seqNoInFlight[parentIndex].pSelf = &this->seqNoInFlight[parentIndex];

         slot = &this->seqNoInFlight[parentIndex];
      }

      // remove the "minimal" element
      this->inFlightSize--;
      swap(this->seqNoInFlight[0], this->seqNoInFlight[this->inFlightSize]);
      *this->seqNoInFlight[0].pSelf = &this->seqNoInFlight[0];
      *this->seqNoInFlight[this->inFlightSize].pSelf = &this->seqNoInFlight[this->inFlightSize];

      // move the new root down to restore the heap property
      {
         unsigned i;
         for (i = 0; ;)
         {
            const unsigned leftChild = 2 * i + 1;
            const unsigned rightChild = 2 * i + 2;
            unsigned minNode = i;

            if (leftChild < this->inFlightSize
                  && this->seqNoInFlight[leftChild].value < this->seqNoInFlight[minNode].value)
               minNode = leftChild;
            if (rightChild < this->inFlightSize
                  && this->seqNoInFlight[rightChild].value < this->seqNoInFlight[minNode].value)
               minNode = rightChild;

            if (minNode != i)
            {
               swap(this->seqNoInFlight[i], this->seqNoInFlight[minNode]);

               *this->seqNoInFlight[i].pSelf = &this->seqNoInFlight[i];
               *this->seqNoInFlight[minNode].pSelf = &this->seqNoInFlight[minNode];
               i = minNode;
            }
            else
            {
               break;
            }
         }
      }
   }
   mutex_unlock(&this->mtx);

   up(&this->slotsAvail);
   MirrorBuddyGroup_put(this);
}

void MirrorBuddyGroup_setSeqNoBase(MirrorBuddyGroup* this, uint64_t seqNoBase)
{
   mutex_lock(&this->mtx);
   if (this->sequence < seqNoBase)
      this->sequence = seqNoBase;
   mutex_unlock(&this->mtx);
}

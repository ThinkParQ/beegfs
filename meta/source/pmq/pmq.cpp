#include "pmq_common.hpp"
#include "pmq.hpp"

static constexpr uint64_t PMQ_SLOT_SIZE = 128;
static constexpr uint64_t PMQ_SLOT_HEADER_SIZE = 16;
static constexpr uint64_t PMQ_SLOT_SPACE = PMQ_SLOT_SIZE - PMQ_SLOT_HEADER_SIZE;

// A bit somewhere in the slot header that indicates that a given slot is the
// first slot of a sequence of slots that hold a message.
static constexpr uint64_t PMQ_SLOT_LEADER_MASK = 1;

static constexpr uint64_t PMQ_CHUNK_SHIFT = 16;
static constexpr uint64_t PMQ_CHUNK_SIZE = (uint64_t) 1 << PMQ_CHUNK_SHIFT;


class CSN_Tag{};
class SSN_Tag{};
class MSN_Tag{};

using CSN = SN<CSN_Tag>;
using SSN = SN<SSN_Tag>;
using MSN = SN<MSN_Tag>;

struct PMQ_Chunk_Hdr
{
   // msn: msn of first message stored in this chunk.
   // msgoffsets_off: offset in the chunk to an array of (msgcount + 1) offsets.
   MSN msn;
   uint16_t msgcount;
   uint16_t msgoffsets_off;
   uint32_t pad;  // pad to 16 bytes for now
};

struct PMQ_Slot
{
   uint32_t flags;
   uint32_t msgsize;
   uint32_t pad0;
   uint32_t pad1;
   char payload[PMQ_SLOT_SPACE];

   __pmq_artificial_method
   Untyped_Slice payload_untyped_slice()
   {
      return Untyped_Slice(payload, sizeof payload);
   }
};

/* In-memory enqueue buffer. This is where incoming message get written first.
 * It consists of a ringbuffer of fixed-size slots.
 * A slot has a header and a payload. The size of each slot is PMQ_SLOT_SIZE,
 * and the payload can be up to up to PMQ_SLOT_SPACE bytes.
 * In the future, we might support even concurrent writes.
 * This structure needs no locking; its contents are static except when
 * initialization and destroying.
 * Accessing the cursors though needs a mutex lock.
 */
struct In_Queue
{
   uint64_t slot_count = 0;
   uint64_t size_bytes = 0;  // slot_count * PMQ_SLOT_SIZE

   // When reaching this fill level (in slots) we persist unconditionally.
   uint64_t slots_persist_watermark = 0;

   // A shared memory file store that survives application restarts (but not
   // OS restarts).
   Posix_FD shm_fd;

   // Memory mapping of @shm_fd.
   MMap_Region mapping;

   Ringbuffer<SSN_Tag, PMQ_Slot> slots;  // the buffer from the mapping.
};

/* Cursors published by the enqueuer. The ssn_* members index into the
 * In_Queue. They are consumed by the persister and by reader threads.
 * They get written by enqueuer threads only.
 *
 * NOTE: The *_disk cursors are treated as "tail" cursors i.e. mark the end of
 * the valid region of the queue. They are a copy from the persister's cursors.
 * They get copied only from time to time as necessary if the In_Queue ran out
 * of space to store new messages: We have ssn_disk <= ssn_mem and msn_disk <=
 * msn. The cursors indicate how far the persister has come in persisting
 * chunks by compacting messages (stored in slots) from the In_Queue.
 */
struct In_Queue_Cursors
{
   MSN msn;
   SSN ssn_mem;
   MSN msn_disk;
   SSN ssn_disk;
};

/* An in-memory representation for a chunk page that is about to be written to
 * disk.
 * Contains an untyped buffer and minimal book-keeping information.
 */
struct Chunk_Buffer
{
   void *data;

   // Tracking msn and ssn of first message so we can know how many pages
   // to write() or fsync().
   MSN msn;
   SSN ssn;

   // last_msn and last_ssn: These fields indicate the one-past-last msn and
   // ssn. They get set only when the chunk buffer gets finalized.
   // The purpose is to allow the persister thread to update the persister
   // cursors after persisting the chunk buffer.
   // Because msn's and ssn's are continuous (i.e. last_msn is equal to the
   // next buffer's msn field), these data fields may seem redundant -- the
   // persister could use the msn and ssn from the following chunk buffer
   // instead.
   // However, that approach has in the past caused a bug where the persister
   // used the cursors from the following buffer before that buffer was even
   // initialized -- effectively decreasing the cursors to an earlier state
   // instead of advancing them.
   // After finding the bug, it was clear that we should introduce a little bit
   // of redundancy in order to keep things simple: the persister will access
   // only finalized buffers, and those will always have last_msn and last_ssn
   // fields set correctly.

   MSN last_msn;
   SSN last_ssn;

   __pmq_artificial_method
   Untyped_Slice untyped_slice() const
   {
      return Untyped_Slice(data, PMQ_CHUNK_SIZE);
   }

   __pmq_artificial_method
   PMQ_Chunk_Hdr *get_chunk_header() const
   {
      return (PMQ_Chunk_Hdr *) data;
   }
};

/* A queue of in-memory chunks.
 * We might write() only one chunk at a time, but we need to hold each chunk in
 * memory until it is fsynced, so we need a bunch of chunk buffers.
 * */
struct Chunk_Queue
{
   // The only purpose of this is to be a "holder" for the data buffers
   // contained in the "chunks" Chunk_Buffer's.
   // It's currently a single contiguous mmap allocation, i.e. (PMQ_CHUNK_SIZE
   // * chunks.slot_count())
   MMap_Region chunk_buffer_mapping;

   // The only purpose of this is to be a "holder" for the Chunk_Buffer
   // structures (the "chunks" Ringbuffer is non-owning).
   Alloc_Slice<Chunk_Buffer> chunks_alloc_slice;

   Ringbuffer<CSN_Tag, Chunk_Buffer> chunks;

   /* CSN, MSN, SSN of the "current" next message that will be compacted. */
   MSN cq_msn;
   CSN cq_csn;
   SSN cq_ssn;

   // Construction data for currently built chunk

   // buffer of current chunk (current chunk is identified Persist_Cursors::cq_csn)
   Chunk_Buffer *ck_buffer;

   uint64_t msg_count;  // number of messages compacted in current chunk

   /* Array of message offsets. msg_count + 1 elements are always valid. The
    * next message will be appended (if it fits) at an offset of
    * offsets[msg_count] bytes in the current chunks page. When the chunk is
    * finalized, the position of the array is set.
    */
   Alloc_Slice<uint16_t> offsets;
};

/* Persistent chunk store -- storing chunks on the file system.
 */
struct Chunk_Store
{
   Posix_FD chunk_fd;

   uint64_t capacity_bytes = 0;

   // After persisting to the chunk store, when at least high_watermark many
   // chunks are filled, we may discard some to lower the chunk fill count to
   // low_watermark.
   // Discarding may also be required in the middle of persisting when all
   // chunks are full. But normally this shouldn't happen because the
   // In_Queue's capacity should be smaller than (chunk_count -
   // high_watermark) chunks.
   // Since discarding chunks involves an fsync() (really a write barrrier
   // would be enough but in practice we only have fsync() currently),
   // we discard many chunks at once to hide the overhead coming from disk
   // latency.
   uint64_t chunks_high_watermark = 0;
   uint64_t chunks_low_watermark = 0;

   __pmq_artificial_method
   uint64_t chunk_count() const
   {
      return capacity_bytes >> PMQ_CHUNK_SHIFT;
   }
};

/* Cursors published by the persister. They index into the chunk store. They
 * are consumed by message readers. Sometimes enqueuer threads read these
 * cursors as well, to be able to skip persisting slots from the In_Queue.
 */
struct Persist_Cursors
{
   // The wal_ssn member indicates the tentative next slot to be written to the
   // In_Queue's persist file.
   // Most slots never end up going to that persist file but are compacted
   // directly to the chunk store. At each sync to disk, only the slots that
   // can't form a complete chunk buffer go to the In_Queue's persist file. In
   // that file, only the slots from cks_ssn to wal_ssn (exclusive) are valid.
   SSN wal_ssn;

   // we also store the MSN corresponding to the wal_ssn.
   MSN wal_msn;

   // We might want to also introduce a cursor to indicate the oldest valid
   // chunk buffer in the (in-memory) queue. But currently only the flusher is
   // reading from the queue -- always at position cks_csn.

   // The next chunk that will be written (and fsync'ed) to disk.
   CSN cks_csn;

   // The msn of the first message that is stored in the chunk indicated by
   // cks_csn. That msn is also stored in that chunk's header.
   MSN cks_msn;

   // The ssn of the leader slot where the first message of the chunk indicated
   // by cks_csn was stored. Note, this ssn is _not_ stored in the chunk's
   // header -- it only has value coordinating with the In_Queue.
   SSN cks_ssn;

   // The next chunk that will be discarded from the chunk store.
   CSN cks_discard_csn;
};

/* This structure is persisted in a state file.
 */
struct Commit_Record
{
   // Number of slots of the in-queue
   // must be a power of 2 currently.
   uint64_t inqueue_slotcount;

   uint64_t slotsfile_size_bytes;

   // next ssn that will hit the slots-persist file
   SSN wal_ssn;
   MSN wal_msn;

   uint64_t chunkfile_size_bytes;

   // Csn, msn, and ssn of the next chunk that will be persisted to the chunk
   // store.
   CSN cks_csn;
   MSN cks_msn;
   SSN cks_ssn;

   // The next chunk that will be discarded from the chunk store
   CSN cks_discard_csn;
};


// Data owned by the enqueuer functionality. There can only be 1 enqueuer
// thread at a time.
struct Enqueuer
{
   PMQ_Enqueuer_Stats enqueuer_stats;
   In_Queue_Cursors in_queue_cursors;
};

// Data owner by the persister functionality. There can only be 1 persister
// thread at a time
struct Persister
{
   PMQ_Persister_Stats stats;
   Persist_Cursors persist_cursors;
};


struct PMQ
{
   PMQ_Owned_String basedir_path;

   Posix_FD basedir_fd;

   Posix_FD slotsfile_fd;
   uint64_t slotsfile_size_bytes = 0;

   In_Queue in_queue;

   // Chunks that get compacted from the in_queue. They will by persisted
   // to the chunk_store.
   Chunk_Queue chunk_queue;

   Chunk_Store chunk_store;

   // Cursors published by enqueuer threads, consumable by persister and reader
   // threads.
   In_Queue_Cursors pub_in_queue_cursors;

   PMQ_PROFILED_MUTEX(pub_in_queue_mutex);
   PMQ_PROFILED_CONDVAR(pub_in_queue_cond);

   // Cursors published by persister threads, consumable by enqueuer and reader
   // threads.
   Mutex_Protected<Persist_Cursors> pub_persist_cursors;
   Mutex_Protected<PMQ_Persister_Stats> pub_persister_stats;


   // must be held to guarantee only 1 enqueuer at a time (protects only
   // In_Queue currently).
   PMQ_PROFILED_MUTEX(enqueue_mutex);

   Enqueuer enqueuer;


   // Must be held to to guarantee only 1 persister at a time. Flushing may
   // happen by a dedicated persister thread that checks regularly, or by an
   // enqueuer thread when the In_Queue is full.

   PMQ_PROFILED_MUTEX(persist_mutex);

   Persister persister;

   Posix_FD statefile_fd;
};

void pmq_get_stats(PMQ *q, PMQ_Stats *out_stats)
{
   PMQ_Stats stats = {};
   stats.persister = q->pub_persister_stats.load();
   {
      PMQ_PROFILED_LOCK(lock_, q->enqueue_mutex);
      stats.enqueuer = q->enqueuer.enqueuer_stats;
   }

   *out_stats = stats;
}

PMQ_Persist_Info pmq_get_persist_info(PMQ *q)
{
   Persist_Cursors persist_cursors = q->pub_persist_cursors.load();
   PMQ_Persist_Info out;
   out.wal_msn = persist_cursors.wal_msn.value();
   out.cks_msn = persist_cursors.cks_msn.value();
   out.cks_discard_csn = persist_cursors.cks_discard_csn.value();
   return out;
}

static bool pmq_persist_finished_chunk_buffers(PMQ *q);

// Called only on initialization and then subsequently by pmq_switch_to_next_chunk_buffer()
// Note: must be called from a persister context (with persist_mutex locked)
static bool pmq_begin_current_chunk_buffer(PMQ *q)
{
   PMQ_PROFILED_FUNCTION;

   Persist_Cursors *pc = &q->persister.persist_cursors;
   Chunk_Queue *cq = &q->chunk_queue;

   uint64_t num_chunks = cq->chunks_alloc_slice.capacity();

   if (cq->cq_csn - pc->cks_csn == num_chunks)
   {
      if (! pmq_persist_finished_chunk_buffers(q))
         return false;
   }

   Chunk_Buffer *ck_buffer = cq->chunks.get_slot_for(cq->cq_csn);
   ck_buffer->msn = cq->cq_msn;
   ck_buffer->ssn = cq->cq_ssn;
   ck_buffer->last_msn = MSN(0);  // only set when chunk buffer gets finalized
   ck_buffer->last_ssn = SSN(0);  // only set when chunk buffer gets finalized

   cq->ck_buffer = ck_buffer;
   cq->msg_count = 0;
   cq->offsets[0] = 16; // XXX ???

   return true;
}

static void pmq_finalize_current_chunk_buffer(PMQ *q)
{
   Chunk_Queue *cq = &q->chunk_queue;

   Chunk_Buffer *cb = cq->ck_buffer;
   cb->last_msn = cq->cq_msn;
   cb->last_ssn = cq->cq_ssn;
   pmq_assert(cq->cq_msn == cb->msn + cq->msg_count);

   pmq_debug_f("Finalize chunk %" PRIu64 ": MSN %" PRIu64 " - %" PRIu64
         ", %" PRIu64 " messages. Last msg ends at %" PRIu64,
         cq->cq_csn.value(),
         cb->msn.value(),
         cb->last_msn.value(),
         cq->msg_count, (uint64_t) cq->offsets[cq->msg_count]);

   // msn: msn of first message stored in this chunk.
   // msgoffsets_off: offset in the chunk to an array of (msgcount + 1) offsets.

   Untyped_Slice chunk_slice = cq->ck_buffer->untyped_slice();
   Slice<uint16_t> offsets_slice = cq->offsets.slice().sub_slice(0, cq->msg_count + 1);

   PMQ_Chunk_Hdr *hdr = (PMQ_Chunk_Hdr *) chunk_slice.data();
   hdr->msn = cq->ck_buffer->msn;
   hdr->msgcount = cq->msg_count;
   hdr->msgoffsets_off = PMQ_CHUNK_SIZE - offsets_slice.size_in_bytes();

   pmq_debug_f("Place msgoffsets array in chunk at bytes offset %" PRIu64,
         (uint64_t) hdr->msgoffsets_off);

   // zero out gap between last message and message-offsets-array
   zero_out_slice(chunk_slice
         .limit_size_bytes(hdr->msgoffsets_off)
         .offset_bytes(offsets_slice.at(cq->msg_count)));

   Untyped_Slice chunk_offsets_slice = chunk_slice.offset_bytes(hdr->msgoffsets_off);
   copy_slice(chunk_offsets_slice, offsets_slice.untyped());
}

// Finalize the current chunk buffer and start the next one.
static bool pmq_switch_to_next_chunk_buffer(PMQ *q)
{
   Chunk_Queue *cq = &q->chunk_queue;

   pmq_finalize_current_chunk_buffer(q);

   // "ship"
   cq->cq_csn += 1;

   if (! pmq_begin_current_chunk_buffer(q))
      return false;

   pmq_assert(cq->ck_buffer->msn.value() == cq->cq_msn.value());

   return true;
}

static bool pmq_msg_fits_current_chunk_buffer(PMQ *q, uint64_t msgsize)
{
   Chunk_Queue *cq = &q->chunk_queue;

   // Compute start of offsets-array given the number of messages in the chunk.
   // Why (msg_count + 2)? Here is why: (msg_count + 2) = (next_count + 1).
   // next_count = the count, after appending current message.
   // Add 1 to that because the offsets array holds one more slot to include
   // the final size.
   uint64_t offsets_off = PMQ_CHUNK_SIZE - (cq->msg_count + 2) * (uint64_t) sizeof cq->offsets[0];

   uint64_t msgs_end = cq->offsets[cq->msg_count] + msgsize;

   return msgs_end <= offsets_off;
}

// Helper function for pmq_persist()
// NOTE: persist_mutex must be locked
static bool pmq_compact(PMQ *q, SSN compact_ssn, SSN max_ssn)
{
   if (false) // NOLINT
   {
      pmq_debug_f("pmq_persist(ssn=%" PRIu64 ", max_ssn=%" PRIu64 ")",
            compact_ssn.value(), max_ssn.value());
   }

   PMQ_PROFILED_FUNCTION;

   Chunk_Queue *cq = &q->chunk_queue;

   for (;;)
   {
      if (sn64_ge(cq->cq_ssn, max_ssn))
      {
         return true;
      }

      // Extract message size from slot header.
      uint64_t msgsize;
      {
         SSN ssn = cq->cq_ssn;
         const PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn);
         if ((slot->flags & PMQ_SLOT_LEADER_MASK) == 0)
         {
            // Earlier there was an assert() here instead of an integrity check,
            // assuming that RAM should never be corrupted. However, the RAM might
            // be filled from disk, and we currently don't validate the data after
            // loading. Thus we now consider slot memory just as corruptible as
            // disk data.
            pmq_perr_f("slot %" PRIu64 " is not a leader slot.", ssn.value());
            return false;
         }
         msgsize = slot->msgsize;
      }

      // check if there is enough room for the message in current chunk buffer.
      // If necessary, start a new chunk buffer. This in turn may require
      // flushing another chunk buffer to disk (we may flush multiple
      // considering throughput vs latency).
      if (! pmq_msg_fits_current_chunk_buffer(q, msgsize))
      {
         if (! pmq_switch_to_next_chunk_buffer(q))
         {
            return false;
         }

         // Actually let's always try to compact up to max_ssn
         // sice we should avoid writing small batches.
         // So disabling this early return.
         if(false) // NOLINT
         if (sn64_ge(cq->cq_ssn, compact_ssn))
         {
            return true;
         }
      }

      // compute number of slots that we need to read, and copy to out-message
      uint64_t nslots_req = (msgsize + PMQ_SLOT_SPACE - 1) / PMQ_SLOT_SPACE;

      uint64_t nslots_avail = max_ssn - cq->cq_ssn;
      if (nslots_req > nslots_avail)
      {
         pmq_perr_f("Internal error: Invalid msgsize field in the slots file!");
         pmq_perr_f("Internal error: msgsize is %" PRIu64 ", needs %" PRIu64
               " slots but I believe only %" PRIu64 " are available!",
               msgsize, nslots_req, nslots_avail);
         return false; // can't we do a little more to handle the issue?
      }

      // copy one message
      uint64_t remain = msgsize;
      uint64_t dst_offset = cq->offsets[cq->msg_count];

      for (uint64_t i = 0; i < nslots_req; i++)
      {
         // copy slots to chunks
         SSN ssn = cq->cq_ssn + i;
         const PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn);

         void *dst = (char *) cq->ck_buffer->data + dst_offset;
         const void *src = __pmq_assume_aligned<16>(slot->payload);
         uint64_t n = remain < PMQ_SLOT_SPACE ? remain : PMQ_SLOT_SPACE;
         memcpy(dst, src, n);
         dst_offset += n;
      }

      cq->cq_ssn += nslots_req;
      cq->cq_msn += 1;

      cq->offsets[cq->msg_count + 1] = cq->offsets[cq->msg_count] + msgsize;
      cq->msg_count += 1;
   }

   return true;
}

static bool pmq_commit(PMQ *q);  // forward decl. We should get rid of this and fix the order.

// Helper function for pmq_persist()
// NOTE: persister lock must be taken!
// Persists all chunk buffers from disk_csn up to (but excluding) cq_csn.
static bool pmq_persist_finished_chunk_buffers(PMQ *q)
{
   Chunk_Queue *cq = &q->chunk_queue;
   Persist_Cursors *pc = &q->persister.persist_cursors;

   pmq_assert(cq->cq_csn - pc->cks_csn <= q->chunk_queue.chunks.slot_count());

   CSN cq_csn = cq->cq_csn;

   uint64_t chunk_slot_mask = pmq_mask_power_of_2(q->chunk_store.chunk_count());

   for (CSN csn = pc->cks_csn;
         csn != cq_csn;)
   {
      pmq_assert(csn - pc->cks_discard_csn <= q->chunk_store.chunk_count());
      if (csn - pc->cks_discard_csn == q->chunk_store.chunk_count())
      {
         // All chunks are filled. This should rarely happen since chunks are
         // normally discarded when the high-watermark chunk fill count is
         // reached. Typically, the chunk store is much larger than the
         // In_Queue, and we should not get here.
         // We discard some chunks manually by calling pmq_commit().
         if (! pmq_commit(q))
            return false;
      }
      pmq_assert(csn - pc->cks_discard_csn < q->chunk_store.chunk_count());

      Chunk_Buffer *cb = q->chunk_queue.chunks.get_slot_for(csn);

      Pointer<PMQ_Chunk_Hdr> hdr = cb->get_chunk_header();

      pmq_debug_f("Persist chunk buffer csn=%" PRIu64 ", pointer is %p, msn is %" PRIu64,
            csn.value(), cb, hdr->msn.value());

      // write chunk buffer to disk.
      Untyped_Slice slice = cb->untyped_slice();
      pmq_assert(slice.size() == PMQ_CHUNK_SIZE);

      int fd = q->chunk_store.chunk_fd.get();
      uint64_t chunk_slot_index = csn.value() & chunk_slot_mask;
      off_t offset_bytes = chunk_slot_index << PMQ_CHUNK_SHIFT;

      if (! pmq_pwrite_all(fd, slice, offset_bytes, "chunk buffer"))
      {
         // should we publish the new cursors anyway? (see exit code below)
         return false;
      }

      csn += 1;

      // Advance chunk store pointers
      pc->cks_csn = csn;
      pc->cks_ssn = cb->last_ssn;
      pc->cks_msn = cb->last_msn;

      pmq_debug_f("persisted. pc->cks_msn=%" PRIu64 ", pc->cks_ssn=%" PRIu64,
            pc->cks_msn.value(), pc->cks_ssn.value());
   }

   return true;
}

// Helper function for pmq_persist_unpersisted_slots()
// Persist a contiguous sub-range of all slots.
// The *slots* argument must correspond to the slots ringbuffer.
// start_index + count may not exceed the slots size.
static bool pmq_persist_slots_slice(PMQ *q, uint64_t start_index, uint64_t count)
{
   Slice<PMQ_Slot> slots = q->in_queue.slots.as_slice();

   uint64_t offset_bytes = start_index * sizeof (PMQ_Slot);

   Untyped_Slice slice = slots.sub_slice(start_index, count).untyped();

   pmq_debug_f("Persist %" PRIu64 " slots (%zu bytes) starting from %" PRIu64,
         count, slice.size(), start_index);

   bool ret = pmq_pwrite_all(q->slotsfile_fd.get(), slice, offset_bytes, "slots-file");

   if (ret)
   {
      q->persister.stats.wal_flushes += 1;
      q->persister.stats.wal_flush_bytes += slice.size();
   }

   return ret;
}

// Helper function for pmq_persist()
// NOTE: persister lock must be taken!
// This function writes all unpersisted slots to the slots-file. The point of
// not writing them as a chunk is that we only want to write complete chunk
// buffers. The reason is that each chunk gets written once only (append-only),
// and chunks are fixed-size (PMQ_CHUNK_SIZE).  Syncing a complete chunk would
// mean spending at least a whole chunk regardless of the number of messages in
// it.
static bool pmq_persist_unpersisted_slots(PMQ *q, SSN persist_ssn)
{
   PMQ_PROFILED_FUNCTION;

   Persist_Cursors *pc = &q->persister.persist_cursors;

   if (sn64_lt(pc->wal_ssn, pc->cks_ssn))
   {
      // The chunk store contains more recent data than the slots file.
      // Effectively clear slotsfile, by setting the valid range from cks_ssn to cks_ssn
      pc->wal_ssn = pc->cks_ssn;
      pc->wal_msn = pc->cks_msn;
   }

   if (sn64_ge(pc->wal_ssn, persist_ssn))
   {
      return true;
   }

   pmq_debug_f("Persist unpersisted slots from %" PRIu64 " to %" PRIu64, pc->wal_ssn.value(), persist_ssn.value());

   SSN ssn_lo = pc->wal_ssn;
   SSN ssn_hi = persist_ssn;

   uint64_t count = q->in_queue.slots.slot_count();
   uint64_t mask = pmq_mask_power_of_2(count);
   uint64_t i_lo = ssn_lo.value() & mask;
   uint64_t i_hi = ssn_hi.value() & mask;

   bool ret;
   if (i_lo <= i_hi)
   {
      ret = pmq_persist_slots_slice(q, i_lo, i_hi - i_lo);
   }
   else
   {
      ret = pmq_persist_slots_slice(q, i_lo, count - i_lo);
      if (ret)
         ret = pmq_persist_slots_slice(q, 0, i_hi);
   }

   if (! ret)
   {
      pmq_perr_f("Failed to persist to slots-file!");
      return false;
   }

   q->persister.stats.fsync_calls += 1;
   if (fsync(q->slotsfile_fd.get()) < 0)
   {
      pmq_perr_ef(errno, "fsync() of slots file failed");
      return false;
   }

   // Only now we update the cursor.
   pc->wal_ssn = ssn_hi;

   // We also need to update the MSN accordingly. To find the MSN, because the
   // MSN is currently not stored in the slots, instead we count the number of
   // slot leaders.
   // This assumes that all the follower slots have been atomically enqueued
   // with each leader.

   for (SSN ssn = ssn_lo; ssn != ssn_hi; ssn ++)
   {
      PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn);
      if ((slot->flags & PMQ_SLOT_LEADER_MASK))
      {
         pc->wal_msn ++;
      }
   }

   return true;
}

// Helper function, only used by pmq_commit()
// Compute the next value of cks_discard_csn.
// NOTE: persister lock must be taken
static CSN pmq_compute_next_discard_csn(PMQ *q)
{
   Persist_Cursors *pc = &q->persister.persist_cursors;

   uint64_t chunks_count = pc->cks_csn - pc->cks_discard_csn;
   uint64_t low_mark = q->chunk_store.chunks_low_watermark;
   uint64_t high_mark = q->chunk_store.chunks_high_watermark;

   if (chunks_count < high_mark)
      return pc->cks_discard_csn;

   CSN old_discard_csn = pc->cks_discard_csn;
   CSN new_discard_csn = pc->cks_csn - low_mark;
   pmq_debug_f("Discarding chunks from %" PRIu64 " to %" PRIu64,
         old_discard_csn.value(), new_discard_csn.value());

   return new_discard_csn;
}

// persister lock must be taken
bool __pmq_profiled pmq_commit(PMQ *q)
{
   PMQ_PROFILED_FUNCTION;

   {
      q->persister.stats.fsync_calls += 1;
      if (fsync(q->chunk_store.chunk_fd.get()) < 0)
      {
         pmq_perr_ef(errno, "fsync() of chunks file failed");
         return false;
      }
   }

   Persist_Cursors *pc = &q->persister.persist_cursors;

   Commit_Record commit_record;
   commit_record.inqueue_slotcount = q->in_queue.slot_count;
   commit_record.slotsfile_size_bytes = q->slotsfile_size_bytes;
   commit_record.wal_ssn = pc->wal_ssn;
   commit_record.wal_msn = pc->wal_msn;
   commit_record.chunkfile_size_bytes = q->chunk_store.capacity_bytes;
   commit_record.cks_csn = pc->cks_csn;
   commit_record.cks_msn = pc->cks_msn;
   commit_record.cks_ssn = pc->cks_ssn;
   commit_record.cks_discard_csn = pmq_compute_next_discard_csn(q);

   {
      Untyped_Slice slice = Untyped_Slice(&commit_record, sizeof commit_record);

      if (! pmq_pwrite_all(q->statefile_fd.get(), slice, 0, "state.dat file"))
      {
         return false;
      }
   }

   {
      if (fsync(q->statefile_fd.get()) < 0)
      {
         pmq_perr_ef(errno, "fsync() of statefile failed");
         return false;
      }
      q->persister.stats.fsync_calls += 1;
   }

   // Successfully committed the next discard cursor, now we can recycle the
   // released chunks internally.
   pc->cks_discard_csn = commit_record.cks_discard_csn;

   q->pub_persister_stats.store(q->persister.stats);
   q->pub_persist_cursors.store(q->persister.persist_cursors);

   return true;
}

// Persist messages from the In_Queue to the Chunk_Queue, at least until reaching ssn.
// The given max_ssn is the hard stop, a good choice here is the In_Queue's ssn_mem.
// The function isn't able to determine max_ssn as ssn_mem on its own, since it
// may or may not be used from within the enqueuer context.
// NOTE: This function tries to fill the current Chunk_Buffer once it reaches compact_ssn.
static bool pmq_persist(PMQ *q, SSN ssn, SSN max_ssn)
{
   if (! pmq_compact(q, ssn, max_ssn))
      goto error;

   if (! pmq_persist_finished_chunk_buffers(q))
      goto error;

   if (! pmq_persist_unpersisted_slots(q, ssn))
      goto error;

   if (! pmq_commit(q))
      goto error;

   return true;

error:
   pmq_perr_f("Failed to persist slots");
   return false;
}

// Only meant to be called by pmq_sync()
static bool _pmq_sync(PMQ *q)
{
   In_Queue_Cursors ic;
   PMQ_PROFILED_LOCK(lock_, q->persist_mutex);

   {
      PMQ_PROFILED_SCOPE("wait-fill");
      PMQ_PROFILED_UNIQUE_LOCK(lock_, q->pub_in_queue_mutex);
      for (;;)
      {
         ic = q->pub_in_queue_cursors;
         pmq_assert(sn64_le(ic.ssn_disk, ic.ssn_mem));
         uint64_t slots_fill = ic.ssn_mem - ic.ssn_disk;
         if (slots_fill >= q->in_queue.slots_persist_watermark)
            break;
         auto max_wait_time = std::chrono::milliseconds(50);
         auto wait_result = q->pub_in_queue_cond.wait_for(lock_, max_wait_time);
         if (wait_result == std::cv_status::timeout)
            break;
         q->persister.stats.wakeups += 1;
      }
   }

   if (false) // NOLINT
   {
      pmq_debug_f("ic.ssn_mem is now %" PRIu64, ic.ssn_mem.value());
      uint64_t slots_fill = ic.ssn_mem - ic.ssn_disk;
      pmq_debug_f("slots_fill is now %" PRIu64, slots_fill);
      pmq_debug_f("slots_persist_watermark is %" PRIu64, q->in_queue.slots_persist_watermark);
   }

   if (! pmq_persist(q, ic.ssn_mem, ic.ssn_mem))
      return false;

   q->persister.stats.num_async_flushes += 1;
   return true;
}

// Entry point to persisted all messages that have been successfully enqueued
// so far. Concurrent operations (e.g. pmq_enqueue_msg()) are possible, but may
// not be persisted this time.
bool pmq_sync(PMQ *q)
{
   PMQ_PROFILED_FUNCTION;

   bool ret = _pmq_sync(q);

   if (! ret)
      pmq_perr_f("Failed to pmq_sync()!");

   return ret;
}

// Helper function for pmq_enqueue_msg
// Attempts to make enough room in the In_Queue
// enqueue_mutex must be locked.

static bool __pmq_profiled pmq_prepare_input_slots(PMQ *q, uint64_t nslots_req)
{
   PMQ_PROFILED_FUNCTION;

   In_Queue_Cursors *ic = &q->enqueuer.in_queue_cursors;

   uint64_t slot_count = q->in_queue.slot_count;
   SSN next_ssn_mem = ic->ssn_mem + nslots_req;

   pmq_assert(ic->ssn_mem - ic->ssn_disk <= slot_count);

   if (next_ssn_mem - ic->ssn_disk <= slot_count)
      return true;

   // Update the ssn_disk cursor from the pub_persist_cursors. Those hold the
   // same values as q->persister.persist_cursors, just ever so slightly
   // outdated. This information lets us detect if we can jump out early,
   // without requiring to lock the persister context, which can take a lot of
   // time.
   {
      Persist_Cursors pc = q->pub_persist_cursors.load();
      ic->msn_disk = pc.cks_msn;
      ic->ssn_disk = pc.cks_ssn;
   }

   if (next_ssn_mem - ic->ssn_disk <= slot_count)
      return true;

   // Still not enough room, need to switch to persister context (lock
   // it) and flush some more messages.

   q->enqueuer.enqueuer_stats.buffer_full_count += 1;

   PMQ_PROFILED_LOCK(lock_, q->persist_mutex);

   if (! pmq_persist(q, next_ssn_mem - slot_count, ic->ssn_mem))
   {
      return false;
   }

   if (false) // NOLINT
   {
      Chunk_Queue *cq = &q->chunk_queue;
      Persist_Cursors *pc = &q->persister.persist_cursors;
      pmq_assert(sn64_ge(cq->cq_ssn, ic->ssn_mem));
      pmq_debug_f("ic->ssn_mem: %" PRIu64 ", cq_ssn - cks_ssn: %" PRIu64,
            ic->ssn_mem.value(), cq->cq_ssn.value() - pc->cks_ssn.value());
   }

   if (false) // NOLINT
   {
      SSN old_ssn_disk = ic->ssn_disk;
      SSN new_ssn_disk = q->persister.persist_cursors.cks_ssn;

      pmq_debug_f("Flushed %" PRIu64 " ssns", ic->ssn_disk - old_ssn_disk);

      if (sn64_le(new_ssn_disk, old_ssn_disk))
      {
         pmq_perr_f("Something is wrong: %" PRIu64 ", %" PRIu64,
               old_ssn_disk.value(), ic->ssn_disk.value());
      }
      pmq_assert(slot_count >= (ic->ssn_mem - ic->ssn_disk));
   }

   // Update the ssn_disk cursor from the (locked) persister context.
   {
      ic->msn_disk = q->persister.persist_cursors.cks_msn;
      ic->ssn_disk = q->persister.persist_cursors.cks_ssn;
   }

   pmq_assert(next_ssn_mem - ic->ssn_disk <= slot_count);
   return true;
}

// Helper function for pmq_enqueue_msg().
// Serialize message to In_Queue's memory buffer.
// Expects enqueue_mutex to be taken.
// Expects that there is enough room to serialize the message (pmq_prepare_input_slots())
static void pmq_serialize_msg(PMQ *q, const void *data, size_t size)
{
   PMQ_PROFILED_FUNCTION;

   In_Queue_Cursors *ic = &q->enqueuer.in_queue_cursors;

   SSN ssn_mem = ic->ssn_mem;
   SSN old_ssn_mem = ic->ssn_mem;

   uint64_t slot_count = q->in_queue.slot_count;
   pmq_assert(pmq_is_power_of_2(slot_count));
   uint32_t slot_flags = PMQ_SLOT_LEADER_MASK;

   // write full slots
   size_t i = 0;
   while (i + PMQ_SLOT_SPACE <= size)
   {
      PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn_mem);
      slot->flags = slot_flags;
      slot->msgsize = size - i;

      memcpy(__pmq_assume_aligned<16>(slot->payload), (const char *) data + i, PMQ_SLOT_SPACE);

      ssn_mem += 1;
      i += PMQ_SLOT_SPACE;
      slot_flags &= ~PMQ_SLOT_LEADER_MASK;
   }

   // write last slot
   if (i < size)
   {
      PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn_mem);
      slot->flags = slot_flags;
      slot->msgsize = size - i;
      memcpy(__pmq_assume_aligned<16>(slot->payload), (const char *) data + i, size - i);

      ssn_mem += 1;
   }

   // can bump ssn_mem cursor, publish new cursors field, and release lock now

   ic->ssn_mem = ssn_mem;
   ic->msn += 1;

   q->enqueuer.enqueuer_stats.total_messages_enqueued += 1;
   q->enqueuer.enqueuer_stats.total_bytes_enqueued += size;

   {
      uint64_t new_slot_count = ic->ssn_mem - ic->ssn_disk;
      uint64_t old_slot_count = old_ssn_mem - ic->ssn_disk;

      bool notify =
         old_slot_count < q->in_queue.slots_persist_watermark &&
         new_slot_count >= q->in_queue.slots_persist_watermark;

      {
         PMQ_PROFILED_UNIQUE_LOCK(lock_, q->pub_in_queue_mutex);
         q->pub_in_queue_cursors = *ic;
         if (notify)
            q->pub_in_queue_cond.notify_one();
      }
   }

   pmq_assert(ic->ssn_mem - ic->ssn_disk <= slot_count);
}

bool pmq_enqueue_msg(PMQ *q, const void *data, size_t size)
{
   PMQ_PROFILED_FUNCTION;

   pmq_assert(size > 0);
   uint64_t nslots_req = (size + PMQ_SLOT_SPACE - 1) / PMQ_SLOT_SPACE;

   PMQ_PROFILED_LOCK(lock_, q->enqueue_mutex);
   if (! pmq_prepare_input_slots(q, nslots_req))
      return false;
   pmq_serialize_msg(q, data, size);
   return true;
}



static void pmq_init_chunk_store_size(Chunk_Store *cks, uint64_t capacity_bytes)
{
   pmq_assert(pmq_is_power_of_2(capacity_bytes));
   pmq_assert(capacity_bytes >= PMQ_Megabytes(64));

   cks->capacity_bytes = capacity_bytes;

   uint64_t chunks_count = cks->capacity_bytes >> PMQ_CHUNK_SHIFT;

   // What is a reasonable watermark at which we should start discarding chunks?
   // Note that while discarding a chunk is logically only advancing a CSN cursor,
   // it's very expensive because we have to fsync() that updated cursor to disk.
   // For now, I'm deciding to set them to chunks_count minus 256 resp. 512.
   // On each discard we'll be discarding between (hi_mark - low_mark) and
   // (chunks_count - low_mark) chunks, i.e. between 16 and 32 MiB of data.
   // These values should be fair when targetting a reasonable throughput of
   // 2GB/sec and an fsync() latency of ~5ms.
   cks->chunks_low_watermark = chunks_count - 512;
   cks->chunks_high_watermark = chunks_count - 256;

   pmq_assert(cks->chunks_low_watermark < chunks_count);
   pmq_assert(cks->chunks_high_watermark < chunks_count);
   pmq_assert(cks->chunks_low_watermark < cks->chunks_high_watermark);
}

static bool pmq_init_createnew(PMQ *q, const PMQ_Init_Params *params)
{
   const char *basedir_path = q->basedir_path.get().buffer;

   if (mkdir(basedir_path, 0750) == -1)
   {
      pmq_perr_ef(errno, "Failed to create queue directory %s", basedir_path);
      return false;
   }

   q->basedir_fd = pmq_open_dir(basedir_path);

   if (! q->basedir_fd.valid())
   {
      pmq_perr_ef(errno,
            "Failed to open the directory we created: %s", basedir_path);
      return false;
   }

   // Initialize In_Queue_Cursors to all 0.
   {
      q->enqueuer.in_queue_cursors = In_Queue_Cursors {};
   }

   // Initialize persister cursors to all 0.
   {
      q->persister.persist_cursors = Persist_Cursors {};
   }

   // Create slots-file.
   // The slots-file is called "wal.dat" but it's not really a WAL -- only a
   // buffer to store the rest slots that didn't make a complete chunk page.
   {
      q->slotsfile_fd = pmq_openat_regular_create(q->basedir_fd.get(),
            "wal.dat", O_RDWR, 0644);

      if (! q->slotsfile_fd.valid())
      {
         pmq_perr_ef(errno, "Failed to create slots file (wal.dat)");
         return false;
      }

      //TODO: currently this must be the same size as the in-memory slots buffer. Fix this, we only need a tiny file on disk to persist the remaining slots that
      //didn't fill a complete chunk page.
      q->slotsfile_size_bytes = q->in_queue.size_bytes;

      if (fallocate(q->slotsfile_fd.get(), FALLOC_FL_ZERO_RANGE,
               0, q->slotsfile_size_bytes) == -1)
      {
         pmq_perr_ef(errno, "Failed to fallocate() slots file");
         return false;
      }
   }

   // Create chunk store
   {
      Chunk_Store *cks = &q->chunk_store;

      uint64_t create_size = params->create_size;

      if (create_size == 0)
         create_size = PMQ_Gigabytes(1);  // default to 1 GiB

      if (create_size < PMQ_Megabytes(64))
      {
         pmq_perr_f("PMQ_Init_Params::create_size is invalid: "
               "Must be at least 64 MiB. Requested: %" PRIu64, create_size);
         return false;
      }

      if (! pmq_is_power_of_2(create_size))
      {
         pmq_warn_f("PMQ_Init_Params::create_size is not a power of 2: %" PRIu64, create_size);
         create_size *= 2;
         while (! pmq_is_power_of_2(create_size))
            create_size = create_size & (create_size - 1);
         pmq_warn_f("PMQ_Init_Params::create_size is not a power of 2: rounded up to %" PRIu64, create_size);
      }

      pmq_init_chunk_store_size(cks, create_size);

      cks->chunk_fd = pmq_openat_regular_create(q->basedir_fd.get(),
            "chunks.dat", O_RDWR, 0644);

      if (! cks->chunk_fd.valid())
      {
         pmq_perr_ef(errno, "Failed to create chunks file");
         return false;
      }

      if (fallocate(cks->chunk_fd.get(), FALLOC_FL_ZERO_RANGE,
               0, cks->capacity_bytes) == -1)
      {
         pmq_perr_ef(errno, "Failed to fallocate() chunks file"
               " to size %" PRIu64, cks->capacity_bytes);
         return false;
      }
   }

   // Create state.dat file
   {
      q->statefile_fd = pmq_openat_regular_create(q->basedir_fd.get(),
            "state.dat", O_RDWR, 0644);

      if (! q->statefile_fd.valid())
      {
         pmq_perr_ef(errno, "Failed to open state.dat file");
         return false;
      }

      // Is it ok to try and reuse the pmq_commit() function to initialize the file?
      if (! pmq_commit(q))
         return false;
   }

   // Sync basedir to make sure the new files are persisted.
   {
      if (fsync(q->basedir_fd.get()) == -1)
      {
         pmq_perr_ef(errno, "Error from fsync() on base directory");
         return false;
      }
   }

   return true;
}

static bool __pmq_validate_commit_record_weak_ordering(
      uint64_t sn_lo, uint64_t sn_hi, const char *name_lo, const char *name_hi)
{
   if (! _sn64_le(sn_lo, sn_hi))
   {
      pmq_perr_f("Integrity error in state.dat file: We expected %s <= %s"
            " but their values are %" PRIu64 " > %" PRIu64,
            name_lo, name_hi, sn_lo, sn_hi);
      return false;
   }
   return true;
}

template<typename T>
static bool _pmq_validate_commit_record_weak_ordering(
      T sn_lo, T sn_hi, const char *name_lo, const char *name_hi)
{
   return __pmq_validate_commit_record_weak_ordering(
         sn_lo.value(), sn_hi.value(), name_lo, name_hi);
}

#define pmq_validate_commit_record_weak_ordering(cr, lo, hi) \
   _pmq_validate_commit_record_weak_ordering((cr).lo, (cr).hi, #lo, #hi)


static bool pmq_inithelper_check_file_size(
      int fd, uint64_t expected_file_size, const char *what_file)
{
   pmq_assert(fd >= 0);

   struct stat st;

   if (fstat(fd, &st) == -1)
   {
      pmq_perr_ef(errno, "Failed to fstat() %s", what_file);
      return false;
   }

   if (! S_ISREG(st.st_mode))
   {
      pmq_perr_f("Internal error: Expected regular file");
      return false;
   }

   uint64_t actual_file_size = (uint64_t) st.st_size;

   if (actual_file_size != expected_file_size)
   {
      pmq_perr_f("%s has wrong size. Expected: %" PRIu64 ", got: %" PRIu64,
            what_file, expected_file_size, actual_file_size);
      return false;
   }

   return true;
}

static bool pmq_init_loadexisting(PMQ *q)
{
   // Open State File
   {
      q->statefile_fd = pmq_openat_regular_existing(q->basedir_fd.get(),
            "state.dat", O_RDWR);

      if (! q->statefile_fd.valid())
      {
         pmq_perr_ef(errno, "Failed to open state.dat file");
         return false;
      }
   }

   Commit_Record commit_record;

   // Load commit record and store in commit_record
   {
      if (! pmq_pread_all(q->statefile_fd.get(),
               Untyped_Slice(&commit_record, sizeof commit_record),
               0, "state.dat"))
      {
         return false;
      }

      if (! pmq_validate_commit_record_weak_ordering(commit_record, cks_discard_csn, cks_csn))
         return false;

      if (! pmq_validate_commit_record_weak_ordering(commit_record, cks_ssn, wal_ssn))
         return false;

      if (! pmq_validate_commit_record_weak_ordering(commit_record, cks_msn, wal_msn))
         return false;

      {
         uint64_t file_size = commit_record.chunkfile_size_bytes;

         if ((file_size % PMQ_CHUNK_SIZE) != 0)
         {
            pmq_perr_f(
                  "state.dat file contains invalid chunkfile size: "
                  "%" PRIu64 " which is not a multiple of the chunk size "
                  "(%" PRIu64 ")", file_size, PMQ_CHUNK_SIZE);
            return false;
         }

         uint64_t chunks_count = file_size / PMQ_CHUNK_SIZE;
         CSN csn_lo = commit_record.cks_discard_csn;
         CSN csn_hi = commit_record.cks_csn;

         if (csn_hi - csn_lo > chunks_count)
         {
            pmq_perr_f("state.dat cks_discard_csn=%" PRIu64 ", cks_csn=%" PRIu64,
                  csn_lo.value(), csn_hi.value());
            pmq_perr_f(
                  "state.dat file contains invalid chunk cursor positions: "
                  " Their distance exceeds the size of the chunks-file "
                  "(%" PRIu64 " > %" PRIu64 ".", csn_hi - csn_lo, chunks_count);
            return false;
         }
      }

      {
         uint64_t file_size = commit_record.slotsfile_size_bytes;

         if ((file_size % PMQ_SLOT_SIZE) != 0)
         {
            pmq_perr_f(
                  "state.dat file contains invalid slots-file size: "
                  "%" PRIu64 " which is not a multiple of the slot size "
                  "(%" PRIu64 ")", file_size, PMQ_SLOT_SIZE);
            return false;
         }

         uint64_t slots_count = file_size / PMQ_SLOT_SIZE;
         SSN ssn_lo = commit_record.cks_ssn;
         SSN ssn_hi = commit_record.wal_ssn;

         if (ssn_hi - ssn_lo > slots_count)
         {
            pmq_perr_f(
                  "state.dat file contains invalid slot cursor positions: "
                  " Their distance exceeds the size of the slots-file.");
            return false;
         }
      }
   }

   // TODO: Currently the slots-file and the in-memory slots-ringbuffer are the same size
   // Later, make the slots-file smaller (just because it doesn't need to be very big)
   // and be very careful how to load to memory.
   {
      q->slotsfile_size_bytes = commit_record.slotsfile_size_bytes;
      q->slotsfile_fd = pmq_openat_regular_existing(q->basedir_fd.get(),
            "wal.dat", O_RDWR);

      if (! q->slotsfile_fd.valid())
      {
         pmq_perr_ef(errno, "Failed to open slots file (wal.dat)");
         return false;
      }

      if (! pmq_inithelper_check_file_size(q->slotsfile_fd.get(),
               q->slotsfile_size_bytes, "state-file (state.dat)"))
      {
         return false;
      }

      if (! pmq_pread_all(
               q->slotsfile_fd.get(),
               q->in_queue.slots.as_slice().untyped(),
               0, "slots-file (wal.dat)"))
      {
         pmq_perr_f("Failed to read from slots file to in-memory slots ringbuffer");
         return false;
      }
   }

   // Load chunk store
   {
      Chunk_Store *cks = &q->chunk_store;

      pmq_init_chunk_store_size(cks, commit_record.chunkfile_size_bytes);

      cks->chunk_fd = pmq_openat_regular_existing(q->basedir_fd.get(),
            "chunks.dat", O_RDWR);

      if (! cks->chunk_fd.valid())
      {
         pmq_perr_ef(errno, "Failed to open chunks.dat file");
         return false;
      }

      if (! pmq_inithelper_check_file_size(cks->chunk_fd.get(),
               cks->capacity_bytes, "chunk file (chunks.dat)"))
      {
         return false;
      }
   }

   // Initialize In_Queue_Cursors
   {
      In_Queue_Cursors ic;
      ic.msn = commit_record.wal_msn;
      ic.ssn_mem = commit_record.wal_ssn;
      ic.msn_disk = commit_record.cks_msn;
      ic.ssn_disk = commit_record.cks_ssn;

      q->pub_in_queue_cursors = ic;
   }

   // Initialize persister cursors
   {
      Persist_Cursors pc;
      pc.wal_ssn = commit_record.wal_ssn;
      pc.wal_msn = commit_record.wal_msn;
      pc.cks_csn = commit_record.cks_csn;
      pc.cks_msn = commit_record.cks_msn;
      pc.cks_ssn = commit_record.cks_ssn;
      pc.cks_discard_csn = commit_record.cks_discard_csn;

      q->pub_persist_cursors.store(pc);
   }

   return true;
}

static bool pmq_init(PMQ *q, const PMQ_Init_Params *params)
{
   q->basedir_path.set(params->basedir_path);

   const char *basedir_path = q->basedir_path.get().buffer;

   // Set up In_Queue
   // This is currently independent of any database state, so we can do it first.
   {
      // TODO how to find proper size (slot count) for the In_Queue buffer?
      // For most use cases, we don't need extremely high bandwidth, but we
      // should think about making it tunable and come up with recommendations.
      // Or even allow it to be sized dynamically.

      q->in_queue.slot_count = 512 * 1024;  // each slot is 128 bytes
      q->in_queue.size_bytes = q->in_queue.slot_count * PMQ_SLOT_SIZE;
      q->in_queue.slots_persist_watermark = q->in_queue.slot_count / 2;

      pmq_debug_f("in-queue size: %" PRIu64 " (%" PRIu64 " slots)",
            q->in_queue.size_bytes, q->in_queue.slot_count);

      // We could consider making an SHM file here to back the In_Queue memory,
      // making the In_Queue persist across application restarts.
      // This would allow to recover any message that was successfully enqueued
      // to the In_Queue (unless the machine was also restarted or crashed
      // before recovery).  On the other hand, it would require elaborate
      // recovery code.

      if (! q->in_queue.mapping.create(NULL, q->in_queue.size_bytes,
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
      {
         pmq_perr_ef(errno, "Failed to mmap() queue memory");
         return false;
      }

      PMQ_Slot *slots = (PMQ_Slot *) q->in_queue.mapping.get();
      pmq_assert(slots);
      __pmq_assert_aligned(slots, 16);

      q->in_queue.slots.reset(Slice<PMQ_Slot>(slots, q->in_queue.slot_count));
   }


   // Create or load the on-disk database

   q->basedir_fd = pmq_open_dir(basedir_path);

   if (! q->basedir_fd.valid())
   {
      if (! (errno == ENOTDIR || errno == ENOENT))
      {
         pmq_perr_ef(errno, "Failed to open queue directory at %s",
               basedir_path);
         return false;
      }

      pmq_msg_f("No queue directory present at %s", basedir_path);
      pmq_msg_f("Creating new queue directory at %s", basedir_path);

      if (! pmq_init_createnew(q, params))
      {
         pmq_perr_f("Failed to create queue directory at %s", basedir_path);
         return false;
      }
   }
   else
   {
      pmq_msg_f("Loading existing queue from %s", basedir_path);

      if (! pmq_init_loadexisting(q))
      {
         pmq_perr_f("Failed to load queue directory at %s", basedir_path);
         return false;
      }

      if (params->create_size != 0 &&
            params->create_size != q->chunk_store.capacity_bytes)
      {
         pmq_warn_f("NOTE: Configured chunk store size is %" PRIu64
               " bytes, which is different from the size of the existing"
               " chunk store: %" PRIu64 " bytes."
               " The chunk store size configuration is currently only"
               " considered when creating a new chunk store.",
               params->create_size,
               q->chunk_store.capacity_bytes);
      }
   }

   // Set up cursors
   q->enqueuer.in_queue_cursors = q->pub_in_queue_cursors;
   q->persister.persist_cursors = q->pub_persist_cursors.load();

   // Initialize Chunk_Queue
   {
      Chunk_Queue *cq = &q->chunk_queue;
      cq->cq_csn = q->persister.persist_cursors.cks_csn;
      cq->cq_ssn = q->persister.persist_cursors.cks_ssn;
      cq->cq_msn = q->persister.persist_cursors.cks_msn;

      cq->chunks_alloc_slice.allocate(2);  // only 2 chunk buffers

      {
         uint64_t map_size_bytes = cq->chunks_alloc_slice.capacity() * PMQ_CHUNK_SIZE;

         if (! cq->chunk_buffer_mapping.create(NULL, map_size_bytes,
                  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
         {
            pmq_perr_ef(errno,
                  "Failed to mmap() %" PRIu64 " bytes for chunk buffer",
                  map_size_bytes);
            return false;
         }
      }

      cq->chunks.reset(cq->chunks_alloc_slice.slice());
      for (uint64_t i = 0; i < cq->chunks.slot_count(); i++)
      {
         Chunk_Buffer *cb = cq->chunks.get_slot_for(CSN(i));
         cb->data = (char *) cq->chunk_buffer_mapping.get() + (i * PMQ_CHUNK_SIZE);
         // initialized later, anyway...
         cb->msn = MSN(0);
         cb->ssn = SSN(0);
      }

      // current chunk page buffer starts out empty.
      cq->msg_count = 0;

      // Since each message is at least 1 byte large, and requires a 2 byte
      // offset stored as well, we can have no more than this number of
      // messages (and thus offsets) in each chunk.
      cq->offsets.allocate(PMQ_CHUNK_SIZE / 3);

      // Set up for submitting messages to first chunk buffer in the Chunk_Queue
      // NOTE I think this will use the Chunk_Buffer identified by the value of cks_csn
      // after the queue was loaded.
      if (! pmq_begin_current_chunk_buffer(q))
         return false;
   }

   {
      In_Queue_Cursors ic = q->enqueuer.in_queue_cursors;
      pmq_debug_f("in_queue_cursors.msn: %" PRIu64, ic.msn.value());
      pmq_debug_f("in_queue_cursors.ssn_mem: %" PRIu64, ic.ssn_mem.value());
      pmq_debug_f("in_queue_cursors.msn_disk: %" PRIu64, ic.msn_disk.value());
      pmq_debug_f("in_queue_cursors.ssn_disk: %" PRIu64, ic.ssn_disk.value());
   }

   {
      Chunk_Queue *cq = &q->chunk_queue;
      pmq_debug_f("chunk_queue.cq_csn: %" PRIu64, cq->cq_csn.value());
      pmq_debug_f("chunk_queue.cq_msn: %" PRIu64, cq->cq_msn.value());
      pmq_debug_f("chunk_queue.cq_ssn: %" PRIu64, cq->cq_ssn.value());
   }

   {
      Persist_Cursors pc = q->persister.persist_cursors;
      pmq_debug_f("persister.wal_ssn: %" PRIu64, pc.wal_ssn.value());
      pmq_debug_f("persister.wal_msn: %" PRIu64, pc.wal_msn.value());
      pmq_debug_f("persister.cks_csn: %" PRIu64, pc.cks_csn.value());
      pmq_debug_f("persister.cks_msn: %" PRIu64, pc.cks_msn.value());
      pmq_debug_f("persister.cks_ssn: %" PRIu64, pc.cks_ssn.value());
      pmq_debug_f("persister.cks_discard_csn: %" PRIu64, pc.cks_discard_csn.value());
   }

   return true;
}

PMQ *pmq_create(const PMQ_Init_Params *params)
{
   PMQ *q = new PMQ();
   if (! q)
      return nullptr;
   if (! pmq_init(q, params))
   {
      delete q;
      return nullptr;
   }
   return q;
}

// This function makes sure that all messages currently written are synced to disk.
// When calling this function, all concurrent access (e.g. pmq_enqueue_msg())
// must have returned and no new ones may be done.
void pmq_destroy(PMQ *q)
{
   if (! pmq_sync(q))
   {
      pmq_warn_f("Failed to sync the queue before shutting it down");
   }
   delete q;
}




/* PMQ_Reader */

// To be able to read the newest messages, which may not be persisted to the
// chunk store but only to the slot-file / in_queue, we need multiple read modes.
// We keep track of where we're reading explicitly because we need to reset the
// appropriate cursors whenever we're switching between the modes.
enum PMQ_Read_Mode
{
   PMQ_Read_Mode_Chunkstore,
   PMQ_Read_Mode_Slotsfile,
};

// read state for reading from the slots file
struct PMQ_Slots_Readstate
{
   SSN ssn;
};

// read state for reading from the chunk store
struct PMQ_Chunks_Readstate
{
   // tracks the current csn
   CSN cnk_csn;

   // tracks whether the chunk indicated by cnk_csn is loaded.
   bool cnk_loaded;

   // This data is extracted from chunk_page (only valid if cnk_loaded)
   MSN cnk_msn;
   uint64_t cnk_msgcount;
   Alloc_Slice<unsigned char> cnk_buffer;
   Slice<uint16_t> cnk_msgoffsets;  // msg-offsets (a subrange inside the cnk_buffer)
};

struct PMQ_Reader
{
   PMQ *q;

   // to prevent races, the reader has its own copy of the Persist_Cursors.
   // They get updated only before reading a message.
   Persist_Cursors persist_cursors;

   // the MSN of the next message we're going to read.
   // Gets incremented each time pmq_read_msg() is called.
   MSN msn;

   PMQ_Read_Mode read_mode;

   // Place to store an error. This will prevent reading after an error.
   // Subsequent seeking (if successful) will clear the error.
   PMQ_Read_Result last_result;

   PMQ_Slots_Readstate slots_readstate;
   PMQ_Chunks_Readstate chunks_readstate;
};

struct PMQ_Msg_Output
{
   void *data;
   // size of the data buffer
   size_t data_size;
   // where the caller wants the size of the message to be written.
   size_t *size_out;

   PMQ_Msg_Output(void *data, size_t data_size, size_t *size_out)
      : data(data), data_size(data_size), size_out(size_out)
   {}
};

static void pmq_reader_update_persist_cursors(PMQ_Reader *reader)
{
   reader->persist_cursors = reader->q->pub_persist_cursors.load();
}

static bool pmq_reader_validate_chunk_hdr(PMQ_Chunks_Readstate *ckread, Pointer<const PMQ_Chunk_Hdr> hdr)
{
   if (hdr->msgcount == 0)
   {
      pmq_perr_f("Read invalid chunk %" PRIu64 ": msgcount is 0.",
            ckread->cnk_csn.value());
      return false;
   }

   uint64_t off_end = (uint64_t) hdr->msgoffsets_off + (hdr->msgcount + 1) * sizeof (uint16_t);
   if (off_end > PMQ_CHUNK_SIZE)
   {
      pmq_perr_f("Read invalid chunk %" PRIu64 ": msg-offsets array exceeds chunk size."
            " msgcount: %" PRIu64 ", msgoffsets_off: %" PRIu64,
            ckread->cnk_csn.value(), (uint64_t) hdr->msgcount, (uint64_t) hdr->msgoffsets_off);
      return false;
   }

   return true;
}

static PMQ_Read_Result pmq_load_chunk(PMQ_Reader *reader)
{
   pmq_assert(reader->read_mode == PMQ_Read_Mode_Chunkstore);

   PMQ_Chunks_Readstate *ckread = &reader->chunks_readstate;

   pmq_assert(!ckread->cnk_loaded);

   PMQ *q = reader->q;

   if (sn64_le(reader->persist_cursors.cks_csn, ckread->cnk_csn))
   {
      return PMQ_Read_Result_EOF;
   }

   Chunk_Store *cks = &q->chunk_store;

   // load chunk
   uint64_t mask = pmq_mask_power_of_2(q->chunk_store.chunk_count());
   uint64_t index = ckread->cnk_csn.value() & mask;
   uint64_t offset = index << PMQ_CHUNK_SHIFT;

   Untyped_Slice buffer_slice = ckread->cnk_buffer.untyped_slice();
   pmq_assert(buffer_slice.size() == PMQ_CHUNK_SIZE);

   if (! pmq_pread_all(
            cks->chunk_fd.get(), buffer_slice, offset, "chunk in chunk file"))
   {
      return PMQ_Read_Result_IO_Error;
   }

   Pointer<const PMQ_Chunk_Hdr> hdr = (PMQ_Chunk_Hdr *) buffer_slice.data();

   // first check if the chunk is still supposed to be there -- it might have
   // been overwritten by the next chunk
   {
      pmq_reader_update_persist_cursors(reader);
      if (sn64_lt(ckread->cnk_csn, reader->persist_cursors.cks_discard_csn))
      {
         pmq_debug_f("LOST SYNC: ckread->cnk_csn=%" PRIu64 ", reader->persist_cursors.cks_discard_csn=%" PRIu64,
               ckread->cnk_csn.value(), reader->persist_cursors.cks_discard_csn.value());
         return PMQ_Read_Result_Out_Of_Bounds;
      }
   }

   if (! pmq_reader_validate_chunk_hdr(ckread, hdr))
   {
      return PMQ_Read_Result_Integrity_Error;
   }

   // Initial validation of the loaded chunk completed.
   // Set variables and return success.

   ckread->cnk_msn = hdr->msn;
   ckread->cnk_msgcount = hdr->msgcount;
   ckread->cnk_msgoffsets = Slice<uint16_t>(
         (uint16_t *) ((char *) ckread->cnk_buffer.data() + hdr->msgoffsets_off),
         hdr->msgcount + 1);
   ckread->cnk_loaded = true;

   // Set the MSN to this chunk's msn too.
   reader->msn = hdr->msn;

   return PMQ_Read_Result_Success;
}

static void pmq_reset_to_specific_chunk(PMQ_Reader *reader, CSN csn)
{
   PMQ_Chunks_Readstate *ckread = &reader->chunks_readstate;
   ckread->cnk_loaded = false;
   ckread->cnk_csn = csn;
}

static PMQ_Read_Result pmq_reset_to_specific_chunk_and_load(
      PMQ_Reader *reader, CSN csn)
{
   pmq_reset_to_specific_chunk(reader, csn);
   return pmq_load_chunk(reader);
}

static void pmq_reader_copy_chunk_header(PMQ_Chunks_Readstate *ckread, PMQ_Chunk_Hdr *out)
{
   *out = *(PMQ_Chunk_Hdr *) ckread->cnk_buffer.data();
}

static bool pmq_check_chunk_msns(CSN csn_lo, CSN csn_hi,
      const PMQ_Chunk_Hdr *hdr_lo, const PMQ_Chunk_Hdr *hdr_hi)
{
   if (sn64_lt(csn_hi, csn_lo))
      return pmq_check_chunk_msns(csn_hi, csn_lo, hdr_hi, hdr_lo);

   MSN cnk_lo_last_msn = hdr_lo->msn + hdr_lo->msgcount;
   MSN cnk_hi_first_msn = hdr_hi->msn;

   if (csn_lo + 1 == csn_hi)
   {
      if (cnk_lo_last_msn != cnk_hi_first_msn)
      {
         pmq_perr_f("Integrity error while reading chunks: MSN %" PRIu64
               " was expected in the chunk following chunk %" PRIu64
               " but found %" PRIu64,
               cnk_lo_last_msn.value(),
               csn_lo.value(),
               cnk_hi_first_msn.value());
         return false;
      }
   }
   else
   {
      // maybe we should check sn64_lt() instead of sn64_le(), because
      // chunks must contain at least 1 message, at least currently.
      if (! sn64_le(cnk_lo_last_msn, cnk_hi_first_msn))
      {
         pmq_perr_f("Integrity error while reading chunks: Chunk SN %" PRIu64
               " < %" PRIu64 " but these chunks have low / high MSNs "
               "%" PRIu64 " >= %" PRIu64,
               csn_lo.value(),
               csn_hi.value(),
               cnk_lo_last_msn.value(),
               cnk_hi_first_msn.value());
         return false;
      }
   }
   return true;
}

static PMQ_Read_Result pmq_bsearch_msg(PMQ_Reader *reader, MSN msn, CSN csn_lo, CSN csn_hi)
{
   if (reader->read_mode != PMQ_Read_Mode_Chunkstore)
   {
      reader->read_mode = PMQ_Read_Mode_Chunkstore;
      reader->chunks_readstate.cnk_loaded = false;
   }

   PMQ_Chunks_Readstate *ckread = &reader->chunks_readstate;

   bool hdr_valid = false;
   PMQ_Chunk_Hdr hdr;
   CSN hdr_csn;

   for (;;)
   {
      CSN csn = csn_lo + (csn_hi - csn_lo) / 2;

      PMQ_Read_Result readres = pmq_reset_to_specific_chunk_and_load(reader, csn);

      if (readres == PMQ_Read_Result_Out_Of_Bounds)
      {
         // Assuming that csn_lo was valid when we were called,
         // we now have a situation where the chunk was concurrently discarded.
         if (csn == csn_lo)
         {
            // Already at the final recursion (csn_lo + 1 == csn_hi). Search
            // space is now empty.
            return PMQ_Read_Result_Out_Of_Bounds;
         }
         // shrink the search space, adapt lower boundary to account for the concurrently discarded data.
         csn_lo = csn + 1;
      }
      else if (readres == PMQ_Read_Result_EOF)
      {
         // Could this happen? I believe not. We're assuming that at the start,
         // csn_lo == csn_hi or csn_hi - 1 was valid.
         assert(0);
      }
      else if (readres != PMQ_Read_Result_Success)
      {
         return readres;
      }
      else
      {
         if (hdr_valid)
         {
            PMQ_Chunk_Hdr old_hdr = hdr;
            CSN old_csn = hdr_csn;

            pmq_reader_copy_chunk_header(ckread, &hdr);
            hdr_csn = csn;

            if (! pmq_check_chunk_msns(csn, old_csn, &hdr, &old_hdr))
            {
               return PMQ_Read_Result_Integrity_Error;
            }
         }
         else
         {
            pmq_reader_copy_chunk_header(ckread, &hdr);
            hdr_csn = csn;
            hdr_valid = true;
         }

         PMQ_Chunks_Readstate *ckread = &reader->chunks_readstate;

         if (sn64_lt(msn, ckread->cnk_msn))
         {
            if (csn == csn_lo)
               // already final iteration
               return PMQ_Read_Result_Out_Of_Bounds;
            csn_hi = csn;
         }
         else if (sn64_ge(msn, ckread->cnk_msn + ckread->cnk_msgcount))
         {
            if (csn == csn_lo)
               // already final iteration
               return PMQ_Read_Result_Out_Of_Bounds;
            csn_lo = csn + 1;
         }
         else
         {
            // message inside this block.
            return PMQ_Read_Result_Success;
         }
      }
   }
}

static PMQ_Read_Result pmq_reader_seek_to_msg_chunkstore(PMQ_Reader *reader, MSN msn)
{
   Persist_Cursors pc = reader->persist_cursors;

   pmq_assert(sn64_le(pc.cks_discard_csn, pc.cks_csn));
   if (pc.cks_discard_csn == pc.cks_csn)
   {
      // The store is empty.
      // Since we already detected that msn is older than pc.cks_msn, we return
      // Out_Of_Bounds, not EOF.
      return PMQ_Read_Result_Out_Of_Bounds;
   }

   CSN csn_lo = pc.cks_discard_csn;
   CSN csn_hi = pc.cks_csn - 1;

   PMQ_Read_Result result = pmq_bsearch_msg(reader, msn, csn_lo, csn_hi);

   if (result != PMQ_Read_Result_Success)
      return result;

   // Currently setting the msn only after the appropriate chunk was found and
   // loaded successfully. We might want to change this later.
   reader->msn = msn;
   return PMQ_Read_Result_Success;
}

struct PMQ_Slot_Header_Read_Result
{
   bool is_leader_slot;
   uint16_t msgsize;
   uint16_t nslots_req;
};

// Helper for function that read slots.
// NOTE: Enqueuer lock must be held!
static PMQ_Read_Result pmq_read_slot_header(PMQ *q, SSN ssn, PMQ_Slot_Header_Read_Result *out)
{
   //XXX this code is copied and adapted from pmq_compact()

   const PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn);

   // Extract message size from slot header.
   out->is_leader_slot = (slot->flags & PMQ_SLOT_LEADER_MASK) != 0;
   out->msgsize = slot->msgsize;
   out->nslots_req = (slot->msgsize + PMQ_SLOT_SPACE - 1) / PMQ_SLOT_SPACE;

   // TODO validate msgsize field, does it make sense?

   return PMQ_Read_Result_Success;
}

// Seek message in the slots file.
// Currently this requires locking the enqueuer and a linear scan.
// We should look for ways to improve.
static PMQ_Read_Result pmq_reader_seek_to_msg_slotsfile(PMQ_Reader *reader, MSN msn)
{
   Persist_Cursors pc = reader->persist_cursors;
   assert(sn64_inrange(msn, pc.cks_msn, pc.wal_msn)); // checked in caller

   std::lock_guard<std::mutex> lock(reader->q->enqueue_mutex);

   // To prevent races, we need to check again using the enqueuer's cursors
   // that the MSN that we're looking for is still in the In_Queue.

   In_Queue_Cursors *ic = &reader->q->enqueuer.in_queue_cursors;
   if (sn64_inrange(msn, ic->msn_disk, ic->msn))
   {
      if (sn64_inrange(pc.wal_msn, msn, ic->msn))
         // this is almost guaranteed but there is a race that should be
         // impossible in practice (requires ic->msn to wrap around between
         // msn and pc.wal_msn).
      {
         MSN msn_cur = ic->msn_disk;
         SSN ssn_cur = ic->ssn_disk;

         while (msn_cur != msn)
         {
            if (sn64_ge(ssn_cur, pc.wal_ssn))
            {
               pmq_perr_f("Integrity Error: Reached end of persisted region in slotsfile "
                     "but did not encounter msn=%" PRIu64, msn.value());
               return PMQ_Read_Result_Integrity_Error;
            }

            PMQ_Slot_Header_Read_Result slot_read_result;

            if (PMQ_Read_Result readres = pmq_read_slot_header(reader->q, ssn_cur, &slot_read_result);
                  readres != PMQ_Read_Result_Success)
            {
                  return readres;
            }

            if (! slot_read_result.is_leader_slot)
            {
               // Earlier there was an assert() here instead of an integrity check,
               // assuming that RAM should never be corrupted. However, the RAM might
               // be filled from disk, and we currently don't validate the data after
               // loading. Thus we now consider slot memory just as corruptible as
               // disk data.
               pmq_perr_f("Integrity Error: slot %" PRIu64 " is not a leader slot.", ssn_cur.value());
               return PMQ_Read_Result_Integrity_Error;
            }

            if (pc.wal_ssn - ssn_cur < slot_read_result.nslots_req)
            {
               pmq_perr_f("Integrity Error: forwarding %d slots through the slots file"
                     " would skip over persisted region", (int) slot_read_result.nslots_req);
               pmq_perr_f("current msn=%" PRIu64 ", ssn=%" PRIu64 ", last valid slot is %" PRIu64,
                     msn_cur.value(), ssn_cur.value(), pc.wal_msn.value());
               return PMQ_Read_Result_Integrity_Error;
            }

            ssn_cur += slot_read_result.nslots_req;
            msn_cur += 1;
         }

         reader->read_mode = PMQ_Read_Mode_Slotsfile;
         reader->slots_readstate.ssn = ssn_cur;
         return PMQ_Read_Result_Success;
      }
   }

   // if we missed the window (race condition) we can expect to find the message in the chunk store.
   return pmq_reader_seek_to_msg_chunkstore(reader, msn);
}

static PMQ_Read_Result pmq_reader_seek_to_msg_impl_real(PMQ_Reader *reader, MSN msn)
{
   pmq_reader_update_persist_cursors(reader);

   Persist_Cursors pc = reader->persist_cursors;

   if (sn64_ge(msn, pc.cks_msn))
   {
      if (sn64_gt(msn, pc.wal_msn))
      {
         return PMQ_Read_Result_Out_Of_Bounds;
      }
      return pmq_reader_seek_to_msg_slotsfile(reader, msn);
   }

   return pmq_reader_seek_to_msg_chunkstore(reader, msn);
}

static PMQ_Read_Result pmq_reader_seek_to_msg_impl(PMQ_Reader *reader, MSN msn)
{
   PMQ_Read_Result result = pmq_reader_seek_to_msg_impl_real(reader, msn);
   reader->last_result = result;

   if (result == PMQ_Read_Result_Success)
   {
      reader->msn = msn;
   }
   else
   {
      pmq_assert(result != PMQ_Read_Result_EOF); // seeking shouldn't return EOF
   }
   return result;
}

PMQ_Read_Result pmq_reader_seek_to_msg(PMQ_Reader *reader, uint64_t msn_value)
{
   MSN msn = MSN(msn_value);
   return pmq_reader_seek_to_msg_impl(reader, msn);
}

PMQ_Read_Result pmq_reader_seek_to_current(PMQ_Reader *reader)
{
   pmq_reader_update_persist_cursors(reader);

   MSN msn = reader->persist_cursors.wal_msn;
   pmq_debug_f("Try seeking to MSN %" PRIu64, msn.value());

   return pmq_reader_seek_to_msg_impl(reader, msn);
}

PMQ_Read_Result pmq_reader_seek_to_csn_impl(PMQ_Reader *reader, CSN csn)
{
   Persist_Cursors *pc = &reader->persist_cursors;

   if (uint64_t chunks_in_store = pc->cks_csn - pc->cks_discard_csn;
         csn - pc->cks_discard_csn >= chunks_in_store)
   {
      if (csn == pc->cks_csn)
      {
         // While we cannot know the msn from a chunk (there are no chunks) we
         // can take the cks_msn instead.
         // This should return EOF but the reader should positioned correctly.
         return pmq_reader_seek_to_msg_impl(reader, pc->cks_msn);
      }
      return PMQ_Read_Result_Out_Of_Bounds;
   }

   // Otherwise, let's load a chunk and read the oldest msn from there.
   // The reader state management should be cleaned up. It's not very clear
   // what all the members mean and how they need to be mutated.

   reader->read_mode = PMQ_Read_Mode_Chunkstore;

   PMQ_Chunks_Readstate *ckread = &reader->chunks_readstate;

   PMQ_Read_Result result = pmq_reset_to_specific_chunk_and_load(reader, csn);

   if (result != PMQ_Read_Result_Success)
   {
      // EOF should not happen because of our prior checks.
      // I would like to use an assert but at least in theory there is the
      // chance of a wraparound happening concurrently.
      if (result == PMQ_Read_Result_EOF)
      {
         // EOF would be misleading since we are not "positioned". Not sure what to do currently.
         result = PMQ_Read_Result_Out_Of_Bounds;
      }
      return result;
   }

   reader->msn = ckread->cnk_msn;
   return PMQ_Read_Result_Success;
}

PMQ_Read_Result pmq_reader_seek_to_oldest(PMQ_Reader *reader)
{
   pmq_reader_update_persist_cursors(reader);

   CSN csn = reader->persist_cursors.cks_discard_csn;
   pmq_debug_f("Try seeking to CSN %" PRIu64, csn.value());

   reader->last_result = pmq_reader_seek_to_csn_impl(reader, csn);
   if (reader->last_result == PMQ_Read_Result_Success)
   {
      pmq_debug_f("Succeeded in seeking to CSN %" PRIu64 ". MSN is %" PRIu64,
            csn.value(), reader->msn.value());
   }
   else
   {
      pmq_debug_f("Seeking to CSN failed");
   }
   return reader->last_result;
}

static PMQ_Read_Result pmq_read_msg_slotsfile(PMQ_Reader *reader, PMQ_Msg_Output output);

// Attempt to read the message given by reader->msn from the chunk store.
// We may have to switch to reading from the slotsfile if we detect an EOF.
static PMQ_Read_Result pmq_read_msg_chunkstore(PMQ_Reader *reader, PMQ_Msg_Output output)
{
   pmq_assert(reader->read_mode == PMQ_Read_Mode_Chunkstore);

   PMQ_Chunks_Readstate *ckread = &reader->chunks_readstate;

   if (! ckread->cnk_loaded)
   {
      PMQ_Read_Result readres = pmq_load_chunk(reader);

      if (readres == PMQ_Read_Result_EOF)
      {
         pmq_debug_f("Reader switches to slots file");
         // Switch to slot-file read mode
         reader->read_mode = PMQ_Read_Mode_Slotsfile;
         reader->slots_readstate.ssn = reader->persist_cursors.cks_ssn;
         return pmq_read_msg_slotsfile(reader, output);
      }

      if (readres != PMQ_Read_Result_Success)
      {
         return readres;
      }

      pmq_assert(ckread->cnk_loaded);
   }
   else if (reader->msn - ckread->cnk_msn == ckread->cnk_msgcount)
   {
      // Load next chunk
      CSN csn_old = ckread->cnk_csn;
      PMQ_Chunk_Hdr hdr_old;
      pmq_reader_copy_chunk_header(ckread, &hdr_old);

      PMQ_Read_Result readres =
         pmq_reset_to_specific_chunk_and_load(reader, ckread->cnk_csn + 1);

      if (readres != PMQ_Read_Result_Success)
         return readres;

      CSN csn_new = ckread->cnk_csn;
      PMQ_Chunk_Hdr hdr_new;
      pmq_reader_copy_chunk_header(ckread, &hdr_new);

      if (! pmq_check_chunk_msns(csn_old, csn_new, &hdr_old, &hdr_new))
      {
         return PMQ_Read_Result_Integrity_Error;
      }
   }

   // Chunk is present
   pmq_assert(sn64_le(ckread->cnk_msn, reader->msn));
   pmq_assert(sn64_lt(reader->msn, ckread->cnk_msn + ckread->cnk_msgcount));
   uint64_t msgindex = reader->msn - ckread->cnk_msn;
   uint64_t msgoff = ckread->cnk_msgoffsets.at(msgindex);
   uint64_t nextoff = ckread->cnk_msgoffsets.at(msgindex + 1);
   uint64_t msgsize = nextoff - msgoff;

   if (msgoff >= nextoff)
   {
      pmq_perr_f("Invalid offsets in chunk %" PRIu64
            ": Offset #%u and #%u are %u > %u",
            ckread->cnk_csn.value(),
            (unsigned) msgindex, (unsigned) msgindex + 1,
            (unsigned) msgoff, (unsigned) nextoff);
      return PMQ_Read_Result_Integrity_Error;
   }

   if (nextoff > PMQ_CHUNK_SIZE)
   {
      pmq_perr_f("Invalid offset in chunk %" PRIu64 ": "
            "Offset #%u = %u exceed chunk size",
            ckread->cnk_csn.value(),
            (unsigned) msgindex + 1, (unsigned) nextoff);
      return PMQ_Read_Result_Integrity_Error;
   }

   *output.size_out = msgsize;

   if (msgsize <= output.data_size)
   {
      Untyped_Slice slice = ckread->cnk_buffer.untyped_slice().offset_bytes(msgoff);
      copy_from_slice(output.data, slice, msgsize);
   }

   reader->msn += 1;

   return PMQ_Read_Result_Success;
}

// Attempt to read the message given by reader->msn from the chunk store.
// We may have to switch to reading from the chunk store if we detect that
// we've lost sync -- this may happen if the message we want to read has
// already disappeared (was overwritten) from the slotsfile.
static PMQ_Read_Result pmq_read_msg_slotsfile(PMQ_Reader *reader, PMQ_Msg_Output output)
{
   pmq_assert(reader->read_mode == PMQ_Read_Mode_Slotsfile);

   PMQ *q = reader->q;
   PMQ_Slots_Readstate *slread = &reader->slots_readstate;
   SSN ssn = slread->ssn;

   if (ssn == reader->persist_cursors.wal_ssn)
   {
      return PMQ_Read_Result_EOF;
   }

   if (sn64_lt(reader->persist_cursors.wal_ssn, ssn))
   {
      pmq_debug_f("sn64_lt(reader->persist_cursors.wal_ssn, ssn): wal_ssn=%" PRIu64 ", ssn=%" PRIu64,
            reader->persist_cursors.wal_ssn.value(), ssn.value());
      // Should we even allow this to happen?
      return PMQ_Read_Result_Out_Of_Bounds;
   }

   // NOTE: We need to be careful to avoid that the ringbuffer slots that we
   // read get overwritten concurrently because of new messages being enqueued.
   // For now we will simply lock the in_queue. We may try to optimize this later.
   // One possible approach could be to check that the slots that we read from
   // are valid -- check it both before and after we read the slots.

   // !!! IDEA !!! instead of locking the in-queue, we could lock the persister.
   // The reason why this should work is that data from the in-queue only gets
   // overwritten after having been persisted.
   // On the other hand, locking the persister might block for an unreasonable
   // amount of time.

   std::lock_guard<std::mutex> lock(q->enqueue_mutex);

   // check that the message we're looking for is still there.
   if (sn64_gt(q->enqueuer.in_queue_cursors.ssn_disk, ssn))
   {
      // The slot was already overwritten before we took the lock.
      // pmq_reader_seek_to_msg() should find the message in the chunk store.
      PMQ_Read_Result readres = pmq_reader_seek_to_msg_impl(reader, reader->msn);

      if (readres != PMQ_Read_Result_Success)
         return readres;

      return pmq_read_msg_chunkstore(reader, output);
   }

   PMQ_Slot_Header_Read_Result slot_read_result;

   {
      PMQ_Read_Result readres = pmq_read_slot_header(q, ssn, &slot_read_result);
      if (readres != PMQ_Read_Result_Success)
         return readres;
   }

   if (! slot_read_result.is_leader_slot)
   {
      // Earlier there was an assert() here instead of an integrity check,
      // assuming that RAM should never be corrupted. However, the RAM might
      // be filled from disk, and we currently don't validate the data after
      // loading. Thus we now consider slot memory just as corruptible as
      // disk data.
      pmq_perr_f("slot %" PRIu64 " is not a leader slot.", ssn.value());
      return PMQ_Read_Result_Integrity_Error;
   }

   *output.size_out = slot_read_result.msgsize;

   if (slot_read_result.msgsize > output.data_size)
      return PMQ_Read_Result_Buffer_Too_Small;

   if (reader->persist_cursors.wal_ssn - ssn < slot_read_result.nslots_req)
   {
      pmq_perr_f("Integrity error: Read inconsistent msgsize from slot");
      return PMQ_Read_Result_Integrity_Error;
   }

   // copy one message
   {
      char *dst = (char *) output.data;
      uint64_t remain = slot_read_result.msgsize;
      for (; remain >= PMQ_SLOT_SPACE;)
      {
         const PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn);
         const char *src = __pmq_assume_aligned<16>(slot->payload);
         memcpy(dst, src, PMQ_SLOT_SPACE);
         ++ ssn;
         dst += PMQ_SLOT_SPACE;
         remain -= PMQ_SLOT_SPACE;
      }
      if (remain)
      {
         const PMQ_Slot *slot = q->in_queue.slots.get_slot_for(ssn);
         const char *src = __pmq_assume_aligned<16>(slot->payload);
         memcpy(dst, src, remain);
         ++ ssn;
      }
   }

   slread->ssn = ssn;
   reader->msn += 1;

   return PMQ_Read_Result_Success;
}

PMQ_Read_Result pmq_read_msg(PMQ_Reader *reader,
      void *data, size_t size, size_t *out_size)
{
   if (reader->last_result != PMQ_Read_Result_Success
         && reader->last_result != PMQ_Read_Result_EOF)
   {
      return reader->last_result;  // need to seek to clear the error!
   }

   PMQ_Msg_Output output(data, size, out_size);

   pmq_reader_update_persist_cursors(reader);

   if (sn64_ge(reader->msn, reader->persist_cursors.wal_msn))
   {
      if (reader->msn == reader->persist_cursors.wal_msn)
      {
         //pmq_debug_f("Reader reaches EOF at msn=%" PRIu64, reader->msn.value());
         return PMQ_Read_Result_EOF;
      }
      return PMQ_Read_Result_Out_Of_Bounds;
   }

   switch (reader->read_mode)
   {
      case PMQ_Read_Mode_Chunkstore:
         pmq_debug_f("Read message %" PRIu64 " from chunk store.", reader->msn.value());
         return pmq_read_msg_chunkstore(reader, output);
      case PMQ_Read_Mode_Slotsfile:
         pmq_debug_f("Read message %" PRIu64 " from slots file.", reader->msn.value());
         return pmq_read_msg_slotsfile(reader, output);
      default:
         // shouldn't happen.
         pmq_assert(0);
         abort();
   }
}

PMQ_Reader *pmq_reader_create(PMQ *q)
{
   PMQ_Reader *reader = new PMQ_Reader;

   if (! reader)
   {
      pmq_perr_f("Failed to allocate reader!");
      return nullptr;
   }

   reader->q = q;
   reader->msn = MSN(0);
   reader->read_mode = PMQ_Read_Mode_Chunkstore;
   reader->last_result = PMQ_Read_Result_Success;
   reader->chunks_readstate.cnk_csn = CSN(0); // for now
   reader->chunks_readstate.cnk_buffer.allocate(PMQ_CHUNK_SIZE);
   reader->chunks_readstate.cnk_loaded = false;
   reader->chunks_readstate.cnk_msn = MSN(0);
   reader->chunks_readstate.cnk_msgcount = 0;
   reader->slots_readstate.ssn = SSN(0);
   return reader;
}

void pmq_reader_destroy(PMQ_Reader *reader)
{
   // TODO?
   delete reader;
}

PMQ *pmq_reader_get_pmq(PMQ_Reader *reader)
{
   return reader->q;
}

uint64_t pmq_reader_get_current_msn(PMQ_Reader *reader)
{
   return reader->msn.value();
}

uint64_t pmq_reader_find_old_msn(PMQ_Reader *reader)
{
   for (uint64_t distance = 1; ; distance = (distance ? 2 * distance : 1))
   {
      pmq_reader_update_persist_cursors(reader);
      Persist_Cursors persist_cursors = reader->persist_cursors;
      CSN csn = persist_cursors.cks_discard_csn + distance;
      if (sn64_ge(csn, persist_cursors.cks_csn))
      {
         return persist_cursors.cks_msn.value();
      }
      // possible optimization: don't load the whole chunk but only the header
      PMQ_Read_Result readres = pmq_reset_to_specific_chunk_and_load(reader, csn);
      if (readres == PMQ_Read_Result_Success)
      {
         return reader->msn.value();
      }
   }
}

PMQ_Persist_Info pmq_reader_get_persist_info(PMQ_Reader *reader)
{
   return pmq_get_persist_info(reader->q);
}

bool pmq_reader_eof(PMQ_Reader *reader)
{
   // this is a bit wacky -- we read the pub_persist_cursors, which requires a mutex lock,
   // because we do not know from the current context if we could just access the Persister State's prive persist_cursors.

   // NOTE: We expect that wal_msn is always kept "in front" of cks_msn (the chunk-store MSN).
   // Even when the wal does not have any additional slots -- in this case, we expect wal_msn == cks_msn.

   pmq_reader_update_persist_cursors(reader);

   MSN wal_msn = reader->persist_cursors.wal_msn;
   return sn64_ge(reader->msn, wal_msn);
}

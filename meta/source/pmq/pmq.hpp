#pragma once

#include <stdint.h>  // uint64_t etc.
#include <stddef.h>  // size_t

struct PMQ_Enqueuer_Stats
{
   // how many times was the buffer filled up (the flusher couldn't keep up)?
   uint64_t buffer_full_count;
   uint64_t total_messages_enqueued;
   uint64_t total_bytes_enqueued;
};

struct PMQ_Persister_Stats
{
   uint64_t num_async_flushes;  // calls to pmq_sync()
   uint64_t wakeups;
   uint64_t fsync_calls;
   uint64_t wal_flushes;
   uint64_t wal_flush_bytes;
};

struct PMQ_Stats
{
   PMQ_Enqueuer_Stats enqueuer;
   PMQ_Persister_Stats persister;
};

struct PMQ;

/* Parmeters for creating a new new queue object (see pmq_create()).
 * If basedir_path exists, try to load existing queue data structures from disk.
 * Otherwise, create the directory and initialize a new queue there.
 * A queue use approximately the number of bytes that were specified in
 * create_size at the time of creation. (Something like 2 GiB is not
 * unreasonable).
 */
struct PMQ_Init_Params
{
   const char *basedir_path;
   uint64_t create_size;
};

PMQ *pmq_create(const PMQ_Init_Params *params);

/* Destroy queue object. This will first flush the remaining buffered messages to disk.
 */
void pmq_destroy(PMQ *q);

bool pmq_enqueue_msg(PMQ *q, const void *data, size_t size);
bool pmq_sync(PMQ *q);

void pmq_get_stats(PMQ *q, PMQ_Stats *stats);

/* Information about persisted data */
struct PMQ_Persist_Info
{
   uint64_t cks_discard_csn;  // oldest CSN in the chunk store (next chunk to be discarded)
   uint64_t cks_msn;  // next MSN to hit the chunk store
   uint64_t wal_msn;  // next MSN to hit the WAL
};

PMQ_Persist_Info pmq_get_persist_info(PMQ *q);

/*
 * Get an updated value of the byte range of the underlying data store.
 * The returned range will be chunk-aligned, but what size chunks are is
 * currently not exposed in this API.
 */


/* */
enum PMQ_Read_Result
{
   // The message was successfully read back.
   PMQ_Read_Result_Success,

   // The provided buffer has insufficient size. (The size gets returned back nevertheless)
   PMQ_Read_Result_Buffer_Too_Small,

   // The requested data is at the current end of the storage area/window. It
   // is the next data the will be written. Try again later.
   // TODO: We might want to introduce mechanisms to block until new data arrives at
   // every level. Currently this has to be implemented in the integration
   // code.
   PMQ_Read_Result_EOF,

   // The requested data is not present. Maybe the requested data was discarded
   // concurrently? It is safe to re-position he cursor and retry.
   PMQ_Read_Result_Out_Of_Bounds,

   // An error was detected by the storage layer
   PMQ_Read_Result_IO_Error,

   // A problem with the data read back from the storage layer was detected.
   PMQ_Read_Result_Integrity_Error,
};

static inline const char *pmq_read_result_string(PMQ_Read_Result readres)
{
   switch (readres)
   {
      case PMQ_Read_Result_Success: return "Success";
      case PMQ_Read_Result_Buffer_Too_Small: return "Buffer_Too_Small";
      case PMQ_Read_Result_EOF: return "EOF";
      case PMQ_Read_Result_Out_Of_Bounds: return "Out_Of_Bounds";
      case PMQ_Read_Result_IO_Error: return "IO_Error";
      case PMQ_Read_Result_Integrity_Error: return "Integrity_Error";
      default: return "(invalid value)";
   }
}


struct PMQ_Reader;

PMQ_Reader *pmq_reader_create(PMQ *q);
void pmq_reader_destroy(PMQ_Reader *reader);

PMQ *pmq_reader_get_pmq(PMQ_Reader *reader);

/* Position cursor at the next incoming message -- or, in other words, at the
 * current write end of the queue. */
PMQ_Read_Result pmq_reader_seek_to_current(PMQ_Reader *reader);

/* Position cursor at the oldest message (the first message in the chunk
 * cks_discard). Note that this is rarely a good idea since this message is
 * likely to be discarded concurrently, so it runs risk of losing sync
 * immediately or shortly. */
PMQ_Read_Result pmq_reader_seek_to_oldest(PMQ_Reader *reader);

/* Position cursor to given msn. MSNs cannot be directly adressed. The
 * implementation will have to load multiple chunks to find it.
 * This also means that the call can fail -- I/O errors etc. can be returned.
 */
PMQ_Read_Result pmq_reader_seek_to_msg(PMQ_Reader *reader, uint64_t msn);

/* Read the current message and advance. On success, returns the size of the
 * message that was read in @out_size and advances to the next message
 * internally.
 */
PMQ_Read_Result pmq_read_msg(PMQ_Reader *reader,
      void *data, size_t size, size_t *out_size);

uint64_t pmq_reader_get_current_msn(PMQ_Reader *reader);

/* Attempt to find the MSN of the oldest persisted message.
 *
 * Note that the MSN that ends up being returned might already be discarded
 * once the caller tries to read that message. So calling this function might
 * not be a good idea.
 *
 * Another difficulty, at the implementation level, is that the implementationn
 * needs to read the oldest chunk to know the oldest MSN in that chunk. But the
 * oldest chunk may be discarded concurrently, so reading it might fail.  In
 * case of a concurrent discard, the implementation will update its
 * oldest-chunk information and then skip ahead some chunks, trying to read a
 * slightly newer chunk. This makes the operation more likely to succeed next
 * time. This continues until either a chunk was read successfully, or we run
 * out of persisted chunks. In the latter case, the implementation returns the
 * current "next" MSN. The PMQ always keeps track of this information, so we
 * can know it without reading a chunk from disk.
 */
uint64_t pmq_reader_find_old_msn(PMQ_Reader *reader);

/* Equivalent to pmq_get_persist_info(pmq_reader_get_pmq(reader)); */
PMQ_Persist_Info pmq_reader_get_persist_info(PMQ_Reader *reader);

/* pmq_reader_eof() -- Inexpensive check if there are messages available
 * currently.
 * This allows a concurrent reader procedure synchronize with writers without
 * having to actually read a message while holding a lock -- which could block
 * writers for a long time if we have to do actual I/O.
 */
bool pmq_reader_eof(PMQ_Reader *reader);



// C++ RAII wrappers

// unique_ptr is maybe not precisely what we're looking for. So we're using some boilerplate instead.
//#include <memory>
//using PMQ_Handle = std::unique_ptr<PMQ, decltype(pmq_destroy)>;
//using PMQ_Reader_Handle = std::unique_ptr<PMQ_Reader, decltype(pmq_reader_destroy)>;


template<typename T, void Deleter(T *)>
class PMQ_Handle_Wrapper
{
   T *m_ptr = nullptr;

public:

   T *get() const
   {
      return m_ptr;
   }

   void drop()
   {
      if (m_ptr)
      {
         Deleter(m_ptr);
         m_ptr = nullptr;
      }
   }

   operator T *() const  // automatic implicit cast to T *
   {
      return m_ptr;
   }
  
   explicit operator bool() const
   {
      return m_ptr != nullptr;
   }

   void operator=(PMQ_Handle_Wrapper&& other)
   {
      drop();
      std::swap(m_ptr, other.m_ptr);
   }

   void operator=(T *ptr)
   {
      drop();
      m_ptr = ptr;
   }

   void operator=(PMQ_Handle_Wrapper const& other) = delete;

   explicit PMQ_Handle_Wrapper(T *ptr = nullptr)
      : m_ptr(ptr)
   {
   }

   ~PMQ_Handle_Wrapper()
   {
      drop();
   }
};

using PMQ_Handle = PMQ_Handle_Wrapper<PMQ, pmq_destroy>;
using PMQ_Reader_Handle = PMQ_Handle_Wrapper<PMQ_Reader, pmq_reader_destroy>;

#include "RetrieveChunksWork.h"

#include <common/net/message/fsck/FetchFsckChunkListMsg.h>
#include <common/net/message/fsck/FetchFsckChunkListRespMsg.h>
#include <common/storage/Storagedata.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/StorageTk.h>

#include <database/FsckDBException.h>
#include <toolkit/FsckException.h>
#include <program/Program.h>

RetrieveChunksWork::RetrieveChunksWork(FsckDB* db, NodeHandle node, SynchronizedCounter* counter,
      AtomicUInt64* numChunksFound, bool forceRestart) :
   log("RetrieveChunksWork"), node(std::move(node)), counter(counter),
   numChunksFound(numChunksFound),
   chunks(db->getChunksTable()), chunksHandle(chunks->newBulkHandle()),
   malformedChunks(db->getMalformedChunksList()),
   forceRestart(forceRestart),
   started(false), startedBarrier(2)
{
}

RetrieveChunksWork::~RetrieveChunksWork()
{
}

void RetrieveChunksWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   log.log(Log_DEBUG, "Processing RetrieveChunksWork");

   try
   {
      doWork();
      // flush buffers before signaling completion
      chunks->flush(chunksHandle);
      // work package finished => increment counter
      this->counter->incCount();

   }
   catch (std::exception& e)
   {
      // exception thrown, but work package is finished => increment counter
      this->counter->incCount();

      // after incrementing counter, re-throw exception
      throw;
   }

   log.log(Log_DEBUG, "Processed RetrieveChunksWork");
}

void RetrieveChunksWork::doWork()
{
   // take the node associated with the current target and send a RetrieveChunksMsg to
   // that node; the chunks are retrieved incrementally
   if ( node )
   {
      std::string nodeID = node->getID();
      FetchFsckChunkListStatus status = FetchFsckChunkListStatus_NOTSTARTED;
      unsigned resultCount = 0;

      do
      {
         FetchFsckChunkListMsg fetchFsckChunkListMsg(RETRIEVE_CHUNKS_PACKET_SIZE, status,
               forceRestart);

         const auto respMsg = MessagingTk::requestResponse(*node, fetchFsckChunkListMsg,
            NETMSGTYPE_FetchFsckChunkListResp);

         if (respMsg)
         {
            auto* fetchFsckChunkListRespMsg = (FetchFsckChunkListRespMsg*) respMsg.get();

            FsckChunkList& chunks = fetchFsckChunkListRespMsg->getChunkList();
            resultCount = chunks.size();

            status = fetchFsckChunkListRespMsg->getStatus();

            // check entry IDs
            for (auto it = chunks.begin(); it != chunks.end(); )
            {
               if (db::EntryID::tryFromStr(it->getID()).first
                     && it->getSavedPath()->str().size() <= db::Chunk::SAVED_PATH_SIZE)
               {
                  ++it;
                  continue;
               }

               ++it;
               malformedChunks->append(*std::prev(it));
               chunks.erase(std::prev(it));
            }

            if (status == FetchFsckChunkListStatus_NOTSTARTED)
            {
               // Another fsck run is still in progress or was aborted, and --forceRestart was not
               // set - this means we can't start a new chunk fetcher.
               started = false;
               startedBarrier.wait();
               startedBarrier.wait();
               return;
            }
            else if (status == FetchFsckChunkListStatus_READERROR)
            {
               throw FsckException("Read error occured while fetching chunks from node; nodeID: "
                  + nodeID);
            }

            if (!started)
            {
               started = true;
               startedBarrier.wait();
               startedBarrier.wait();
            }

            this->chunks->insert(chunks, this->chunksHandle);

            numChunksFound->increase(resultCount);
         }
         else
         {
            throw FsckException("Communication error occured with node; nodeID: " + nodeID);
         }

         if ( Program::getApp()->getShallAbort() )
            break;

      } while ( (resultCount > 0) || (status == FetchFsckChunkListStatus_RUNNING) );
   }
   else
   {
      // basically this should never ever happen
      log.logErr("Requested node does not exist");
      throw FsckException("Requested node does not exist");
   }
}

/**
 * Waits until started conditin is is signalled and returns the value of stared
 * @param isStarted ptr to boolean which is set to whether the server replied it actually started
 *                  the process. Note: this is a ptr and not a return to ensure the member variable
 *                  started is no longer accessed after doWork is finished and the object possibly
 *                  already deleted.
 */
void RetrieveChunksWork::waitForStarted(bool* isStarted)
{
   startedBarrier.wait();
   *isStarted = started;
   startedBarrier.wait();
}

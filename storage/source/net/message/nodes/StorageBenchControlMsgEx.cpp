#include <app/App.h>
#include <common/net/message/nodes/StorageBenchControlMsgResp.h>
#include <components/benchmarker/StorageBenchOperator.h>
#include <program/Program.h>
#include "StorageBenchControlMsgEx.h"

bool StorageBenchControlMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "StorageBenchControlMsg incoming";

   StorageBenchResultsMap results;
   int cmdErrorCode = STORAGEBENCH_ERROR_NO_ERROR;

   App* app = Program::getApp();
   StorageBenchOperator* storageBench = app->getStorageBenchOperator();

   switch(getAction())
   {
      case StorageBenchAction_START:
      {
         cmdErrorCode = storageBench->initAndStartStorageBench(&getTargetIDs(), getBlocksize(),
            getSize(), getThreads(), getODirect(), getType() );
      } break;

      case StorageBenchAction_STOP:
      {
         cmdErrorCode = storageBench->stopBenchmark();
      } break;

      case StorageBenchAction_STATUS:
      {
         storageBench->getStatusWithResults(&getTargetIDs(), &results);
         cmdErrorCode = STORAGEBENCH_ERROR_NO_ERROR;
      } break;

      case StorageBenchAction_CLEANUP:
      {
         cmdErrorCode = storageBench->cleanup(&getTargetIDs());
      } break;

      default:
      {
         LogContext(logContext).logErr("unknown action!");
      } break;
   }

   int errorCode;

   // check if the last command from the fhgfs_cmd was successful,
   // if not send the error code of the command to the fhgfs_cmd
   // if it was successful, send the error code of the last run or acutely run of the benchmark
   if (cmdErrorCode != STORAGEBENCH_ERROR_NO_ERROR)
   {
      errorCode = cmdErrorCode;
   }
   else
   {
      errorCode = storageBench->getLastRunErrorCode();
   }

   ctx.sendResponse(
         StorageBenchControlMsgResp(storageBench->getStatus(), getAction(),
            storageBench->getType(), errorCode, results) );

   return true;
}

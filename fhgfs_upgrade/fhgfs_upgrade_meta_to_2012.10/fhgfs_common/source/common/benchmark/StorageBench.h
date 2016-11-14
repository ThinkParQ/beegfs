#ifndef STORAGEBENCH_H_
#define STORAGEBENCH_H_

#include <common/Common.h>


// defines storage benchmark errors...
#define STORAGEBENCH_ERROR_COM_TIMEOUT                -3
#define STORAGEBENCH_ERROR_ABORT_BENCHMARK            -2
#define STORAGEBENCH_ERROR_WORKER_ERROR               -1
#define STORAGEBENCH_ERROR_NO_ERROR                   0
#define STORAGEBENCH_ERROR_UNINITIALIZED              1

#define STORAGEBENCH_ERROR_INITIALIZATION_ERROR       10
#define STORAGEBENCH_ERROR_INIT_READ_DATA             11
#define STORAGEBENCH_ERROR_INIT_CREATE_BENCH_FOLDER   12
#define STORAGEBENCH_ERROR_INIT_TRANSFER_DATA         13

#define STORAGEBENCH_ERROR_RUNTIME_ERROR              20
#define STORAGEBENCH_ERROR_RUNTIME_DELETE_FOLDER      21
#define STORAGEBENCH_ERROR_RUNTIME_OPEN_FILES         22
#define STORAGEBENCH_ERROR_RUNTIME_UNKNOWN_TARGET     23
#define STORAGEBENCH_ERROR_RUNTIME_IS_RUNNING         24
#define STORAGEBENCH_ERROR_RUNTIME_CLEANUP_JOB_ACTIVE 25


// map for throughput results; key: targetID, value: throughput in kb/s
typedef std::map<uint16_t, int64_t> StorageBenchResultsMap;
typedef StorageBenchResultsMap::iterator StorageBenchResultsMapIter;
typedef StorageBenchResultsMap::const_iterator StorageBenchResultsMapCIter;
typedef StorageBenchResultsMap::value_type StorageBenchResultsMapVal;


/*
 * enum for the action parameter of the storage benchmark
 */
enum StorageBenchAction
{
   StorageBenchAction_START = 0,
   StorageBenchAction_STOP = 1,
   StorageBenchAction_STATUS = 2,
   StorageBenchAction_CLEANUP = 3,
   StorageBenchAction_NONE = 4
};

/*
 * enum for the different benchmark types
 */
enum StorageBenchType
{
   StorageBenchType_READ = 0,
   StorageBenchType_WRITE = 1,
   StorageBenchType_NONE = 2
};

/*
 * enum for the states of the state machine of the storage benchmark operator
 * note: see STORAGEBENCHSTATUS_IS_ACTIVE
 */
enum StorageBenchStatus
{
   StorageBenchStatus_UNINITIALIZED = 0,
   StorageBenchStatus_INITIALISED = 1,
   StorageBenchStatus_ERROR = 2,
   StorageBenchStatus_RUNNING = 3,
   StorageBenchStatus_STOPPING = 4,
   StorageBenchStatus_STOPPED = 5,
   StorageBenchStatus_FINISHING = 6,
   StorageBenchStatus_FINISHED = 7
};

#define STORAGEBENCHSTATUS_IS_ACTIVE(status) ( (status == StorageBenchStatus_RUNNING) || \
   (status == StorageBenchStatus_FINISHING) || (status == StorageBenchStatus_STOPPING) )

#endif /* STORAGEBENCH_H_ */

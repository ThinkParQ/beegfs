#ifndef STORAGETARGETS_H_
#define STORAGETARGETS_H_


#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/threading/SafeRWLock.h>
#include <app/config/Config.h>
#include <storage/TargetOfflineWait.h>
#include <storage/QuotaBlockDevice.h>



#define LAST_BUDDY_COMM_TIMESTAMP_FILENAME "lastbuddycomm"
#define LAST_BUDDY_COMM_OVERRIDE_FILENAME  "lastbuddycomm.override"
#define BUDDY_NEEDS_RESYNC_FILENAME        "buddyneedsresync"


struct StorageTargetData
{
   std::string targetPath;
   TargetConsistencyState consistencyState;
   int chunkFD;
   int mirrorFD;
   QuotaBlockDevice quotaBlockDevice; // quota related information about the block device
   bool buddyResyncInProgress;
   bool cleanShutdown; // Was the previous session cleanly shut down?
};

typedef std::map<uint16_t, StorageTargetData> StorageTargetDataMap;
typedef StorageTargetDataMap::iterator StorageTargetDataMapIter;
typedef StorageTargetDataMap::const_iterator StorageTargetDataMapCIter;

typedef std::map<uint16_t, TargetConsistencyState> ConsistencyStateMap;
typedef ConsistencyStateMap::iterator ConsistencyStateMapIter;
typedef ConsistencyStateMap::const_iterator ConsistencyStateMapCIter;
typedef ConsistencyStateMap::value_type ConsistencyStateMapVal;

typedef std::map<uint16_t, unsigned> TargetNeedsResyncCountMap;

class StorageTargets
{
   public:
      StorageTargets(Config* cfg) throw(InvalidConfigException);
      ~StorageTargets();

      void prepareTargetDirs(Config* cfg) throw(InvalidConfigException);

      bool addTargetDir(uint16_t targetNumID, std::string storagePathStr);

      bool removeTargetDir(uint16_t targetNumID);

      void initQuotaBlockDevices();

      void decideResync(const TargetStateMap& statesFromMgmtd,
         TargetStateMap& outLocalStateChanges);
      void checkBuddyNeedsResync();

      void generateTargetInfoList(const UInt16List& targetIDs,
         StorageTargetInfoList& outStorageTargetInfoList);

      void addStartupTimeout();

      void syncBuddyGroupMap(const UInt16List& buddyGroupIDs, const UInt16List& primaryTargetIDs,
         const UInt16List& secondaryTargetIDs);

      static void fillTargetStateMap(const UInt16List& targetIDs,
         const UInt8List& reachabilityStates, const UInt8List& consistencyStates,
         TargetStateMap& outStateMap);

      static void updateTargetStateLists(const TargetStateMap& stateMap,
         const UInt16List& targetIDs, UInt8List& outReachabilityStates,
         UInt8List& outConsistencyStates);

      TargetConsistencyStateVec getTargetConsistencyStates(UInt16Vector targetIDs);

   private:
      RWLock rwlock;

      StorageTargetDataMap storageTargetDataMap; // keys: targetIDs; values: StorageTargetData

      // Maps targetIDs to their buddy groups.
      typedef std::map<uint16_t, uint16_t> TargetBuddyGroupMap;
      typedef TargetBuddyGroupMap::const_iterator TargetBuddyGroupMapCIter;
      TargetBuddyGroupMap targetBuddyGroupMap;
      RWLock targetBuddyGroupMapLock;

      TargetOfflineWait targetOfflineWait;

      StringList targetPathsList; // paths to storage targetPaths

      IntList targetLockFDs; // for target dirs lock/unlock
      std::string primaryPath; // for first target path in the user-defined list

      AtomicInt16 buddyResyncCount;

      QuotaInodeSupport supportForInodeQuota; // value for the level of inode quota support of the
                                              // blockdevices

      void initTargetPathsList(Config* cfg) throw(InvalidConfigException);

      void prepareTargetPaths(Config* cfg) throw(InvalidConfigException);

      void lockTargetPaths() throw(InvalidConfigException);
      void unlockTargetPaths() throw(InvalidConfigException);

      bool removeTargetDir(std::string storagePathStr, bool removeAll = false);
      bool removeTargetDirUnlocked(uint16_t targetNumID);

      bool removeQuotaBlockDeviceUnlocked(std::string storagePathStr, bool removeAll);

      void handleTargetStateChange(uint16_t targetID, TargetConsistencyState newState);
      void setBuddyNeedsResyncState(uint16_t targetID, bool needsResync, bool targetIDIsBuddy);

      static void getStatInfo(std::string& targetPathStr, int64_t* outSizeTotal,
         int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree);


   public:
      // getters & setters

      /**
       * @return false if targetID unknown
       */
      bool getPath(const uint16_t targetID, std::string* outPath)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         bool retVal;

         StorageTargetDataMapCIter iter = storageTargetDataMap.find(targetID);
         if(likely(iter != storageTargetDataMap.end() ) )
         { // found it
            *outPath = (iter->second).targetPath;
            retVal = true;
         }
         else
         { // target not found
            *outPath = "<unknown_target>";
            retVal = false;
         }

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * @return false if targetID unknown
       */
      bool getState(const uint16_t targetID, TargetConsistencyState& outState)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         bool retVal;

         StorageTargetDataMapCIter iter = storageTargetDataMap.find(targetID);
         if(likely(iter != storageTargetDataMap.end() ) )
         { // found it
            outState = (iter->second).consistencyState;
            retVal = true;
         }
         else
         { // target not found
            retVal = false;
         }

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /*
       * NOTE: if targetID does not exist, nothing happens
       *
       * @return true if target exists, false otherwise.
       */
      bool setState(const uint16_t targetID, TargetConsistencyState state)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         TargetConsistencyState oldState = TargetConsistencyState_GOOD; // Initialized to mute
         TargetConsistencyState newState = TargetConsistencyState_GOOD; // false compiler warning.
         bool found = false;

         StorageTargetDataMapIter iter = storageTargetDataMap.find(targetID);
         if(likely(iter != storageTargetDataMap.end() ) ) // found it
         {
            StorageTargetData& target = iter->second;

            found = true;

            oldState = target.consistencyState;

            target.consistencyState = newState = state;
         }

         safeLock.unlock(); // U N L O C K

         if( found && (oldState != state) )
         {
            handleTargetStateChange(targetID, newState);
         }

         return found;
      }

      /*
       * Sets the state of all targets to a new state.
       */
      void setAllTargetStates(TargetConsistencyState state)
      {
         ConsistencyStateMap changedStates;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         for(StorageTargetDataMapIter iter = storageTargetDataMap.begin();
            iter != storageTargetDataMap.end(); ++iter)
         {
            TargetConsistencyState& currentConsistencyState = iter->second.consistencyState;
            if(currentConsistencyState != state)
            {
               currentConsistencyState = state;
               changedStates.insert(ConsistencyStateMapVal( iter->first, currentConsistencyState) );
            }
         }

         safeLock.unlock(); // U N L O C K

         for(ConsistencyStateMapCIter iter = changedStates.begin();
             iter != changedStates.end(); ++iter)
            handleTargetStateChange(iter->first, iter->second);
      }

      /**
       * @return -1 if target is unknown, otherwise the file descriptor
       */
      int getChunkFD(const uint16_t targetID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         int retVal = getChunkFDUnlocked(targetID);

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * @return -1 if target is unknown, otherwise the file descriptor
       */
      int getChunkFDUnlocked(const uint16_t targetID)
      {
         int retVal;

         StorageTargetDataMapCIter iter = storageTargetDataMap.find(targetID);
         if(likely(iter != storageTargetDataMap.end() ) )
         { // found it
            retVal = (iter->second).chunkFD;
         }
         else
         { // target not found
            retVal = -1;
         }

         return retVal;
      }

      /**
       * @return -1 if target is unknown, otherwise the file descriptor
       */
      int getMirrorFD(const uint16_t targetID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         int retVal = getMirrorFDUnlocked(targetID);

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * @return -1 if target is unknown, otherwise the file descriptor
       */
      int getMirrorFDUnlocked(const uint16_t targetID)
      {
         int retVal;

         StorageTargetDataMapCIter iter = storageTargetDataMap.find(targetID);
         if(likely(iter != storageTargetDataMap.end() ) )
         { // found it
            retVal = (iter->second).mirrorFD;
         }
         else
         { // target not found
            retVal = -1;
         }

         return retVal;
      }

      /**
       * @param isMirrorFD true to get the fd for the mirror subdir, false to get the fd for the
       *    general chunks subdir.
       * @return -1 if target is unknown, otherwise the file descriptor
       */
      int getChunkFDAndConsistencyState(const uint16_t targetID, bool isMirrorFD,
         TargetConsistencyState* outConsistencyState)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         int retVal = -1; // -1 means "not found"

         StorageTargetDataMapCIter iter = storageTargetDataMap.find(targetID);

         if(likely(iter != storageTargetDataMap.end() ) )
         { // found it
            retVal = isMirrorFD ? (iter->second).mirrorFD : (iter->second).chunkFD;
            *outConsistencyState = (iter->second).consistencyState;
         }

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * @return the first path in the user-defined target list
       */
      std::string getPrimaryPath()
      {
         // primaryPath is read-only, so we don't use locking here
         return primaryPath;
      }

      /**
       * @param outTargets copy of all mapped targetPaths (keys: targetIDs; values: paths)
       */
      void getAllTargets(TargetPathMap* outTargets)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         for(StorageTargetDataMapIter iter = storageTargetDataMap.begin();
            iter != storageTargetDataMap.end(); iter++)
         {
            std::pair<uint16_t, std::string> mapElem(iter->first,
               (iter->second).targetPath);
            outTargets->insert(mapElem);
         }

         safeLock.unlock(); // U N L O C K
      }

      /**
       * Get IDs of all currently mapped targetPaths
       */
      void getAllTargetIDs(UInt16List* outTargetIDs)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K


         for(StorageTargetDataMapIter iter = storageTargetDataMap.begin();
            iter != storageTargetDataMap.end(); iter++)
            outTargetIDs->push_back(iter->first);

         safeLock.unlock(); // U N L O C K
      }

      /**
       * Get paths of all currently mapped targetPaths
       */
      void getAllTargetPaths(StringList* outPaths)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         for(StorageTargetDataMapIter iter = storageTargetDataMap.begin();
            iter != storageTargetDataMap.end(); iter++)
            outPaths->push_back((iter->second).targetPath);

         safeLock.unlock(); // U N L O C K
      }

      /**
       * Get the states of all targets managed by this object (i.e. targets local to this server).
       */
      void getAllTargetConsistencyStates(UInt16List& outTargetIDs, UInt8List& outStates)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         for(StorageTargetDataMapCIter iter = storageTargetDataMap.begin();
            iter != storageTargetDataMap.end(); ++iter)
         {
            outTargetIDs.push_back(iter->first);
            outStates.push_back(iter->second.consistencyState);
         }

         safeLock.unlock();
      }

      /**
       * Get the list of configured target paths
       *
       * Note: Other than getAllTargetPaths(), this method returns the raw list of target paths,
       * independet of whether they have been mapped, so this can already be used during App
       * initialization.
       */
      void getRawTargetPathsList(StringList* outTargetPaths)
      {
         *outTargetPaths = targetPathsList;
      }

      /**
       * Get number of mapped targetPaths
       */
      size_t getNumTargets()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         size_t retVal = storageTargetDataMap.size();

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * @returns true if target is in storageTargetDataMap, false otherwise.
       */
      bool isLocalTarget(uint16_t targetID)
      {
         bool res;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         res = storageTargetDataMap.find(targetID) != storageTargetDataMap.end();

         safeLock.unlock();

         return res;
      }

      /**
       * Get the quota related information about the block device of the storage targets
       */
      void getQuotaBlockDevices(QuotaBlockDeviceMap* outQuotaBlockDeviceMap)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         for(StorageTargetDataMapIter iter = storageTargetDataMap.begin();
            iter != storageTargetDataMap.end(); iter++)
         {
            std::pair<uint16_t, QuotaBlockDevice> mapElem(iter->first,
               (iter->second).quotaBlockDevice);
            outQuotaBlockDeviceMap->insert(mapElem);
         }

         safeLock.unlock(); // U N L O C K
      }

      /**
       * Get the quota related information about the block device of the given storage target
       */
      bool getQuotaBlockDevice(QuotaBlockDeviceMap* outQuotaBlockDeviceMap, uint16_t targetNumID,
         QuotaInodeSupport* quotaInodeSupport)
      {
         bool retVal = true;
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         StorageTargetDataMapIter iter = this->storageTargetDataMap.find(targetNumID);
         if(iter != this->storageTargetDataMap.end() )
         {
            std::pair<uint16_t, QuotaBlockDevice> mapElem(iter->first,
               (iter->second).quotaBlockDevice);
            outQuotaBlockDeviceMap->insert(mapElem);
            *quotaInodeSupport = (iter->second).quotaBlockDevice.quotaInodeSupportFromBlockDevice();
         }
         else
            retVal = false;

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      void writeLastBuddyCommTimestamp(uint16_t targetID)
      {
         std::string targetPath;
         std::string filename;

         bool getPathRes = getPath(targetID, &targetPath);
         if(unlikely(!getPathRes) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to write last buddy comm timestamp for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            return;
         }

         filename = targetPath + "/" LAST_BUDDY_COMM_TIMESTAMP_FILENAME;

         int64_t timestampSec = time(NULL);

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         int fd = open(filename.c_str(), O_CREAT | O_TRUNC | O_WRONLY,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

         if(fd == -1)
         { // error
            int errCode = errno;

            LogContext(__func__).logErr(
               "Unable to create timestamp file for last buddy communication; targetID: "
                  + StringTk::uintToStr(targetID) + "; SysErr: " + System::getErrString(errCode));

            safeLock.unlock(); // U N L O C K
            return;
         }

         std::string timestampStr = StringTk::int64ToStr(timestampSec);
         ssize_t writeRes = write(fd, timestampStr.c_str(), timestampStr.length() );
         IGNORE_UNUSED_VARIABLE(writeRes);

         close(fd);

         safeLock.unlock(); // U N L O C K
      }

      /**
       * @param targetID
       * @param outIsOverride set to true if an override file exists; can be NULL if not needed
       *
       * @return the lastbuddycomm timestamp, 0 if file is not found or empty.
       */
      int64_t readLastBuddyCommTimestamp(uint16_t targetID, bool* outIsOverride = NULL)
      {
         int64_t retVal = 0;

         std::string targetPath;
         std::string filename;

         bool getPathRes = getPath(targetID, &targetPath);
         if(unlikely(!getPathRes) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to read last buddy comm timestamp for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            return 0;
         }

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         retVal = readLastBuddyCommTimestampUnlocked(targetPath, outIsOverride);

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      FhgfsOpsErr overrideLastBuddyComm(uint16_t targetID, int64_t timestampSec)
      {
         std::string targetPath;
         std::string lastBuddyCommFilename;
         std::string overrideFilename;

         bool getPathRes = getPath(targetID, &targetPath);
         if(unlikely(!getPathRes))
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to override last buddy comm for unknown target. "
                  "targetID: " + StringTk::uintToStr(targetID));
            return FhgfsOpsErr_UNKNOWNTARGET;
         }

         lastBuddyCommFilename = targetPath + "/" LAST_BUDDY_COMM_TIMESTAMP_FILENAME;
         overrideFilename = targetPath + "/" LAST_BUDDY_COMM_OVERRIDE_FILENAME;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         // we try to move the original file first, because we might be out of disk space and
         // moving the old file doesn't need additional space
         int fd;
         int renameRes = rename(lastBuddyCommFilename.c_str(), overrideFilename.c_str());
         if(likely(renameRes == 0))
            fd = open(overrideFilename.c_str(), O_TRUNC | O_WRONLY);
         else
            fd = open(overrideFilename.c_str(), O_CREAT | O_WRONLY,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

         if(fd == -1)
         { // error
            int errCode = errno;

            LogContext(__func__).logErr(
               "Unable to override timestamp for last buddy communication; targetID: "
                  + StringTk::uintToStr(targetID) + "; SysErr: " + System::getErrString(errCode));

            safeLock.unlock(); // U N L O C K
            return FhgfsOpsErr_UNKNOWNTARGET;
         }

         std::string timestampStr = StringTk::int64ToStr(timestampSec);
         ssize_t writeRes = write(fd, timestampStr.c_str(), timestampStr.length());
         IGNORE_UNUSED_VARIABLE(writeRes);

         close(fd);

         safeLock.unlock(); // U N L O C K

         return FhgfsOpsErr_SUCCESS;
      }

      void rmLastBuddyCommOverride(uint16_t targetID)
      {
         std::string targetPath;
         std::string overrideFilename;

         bool getPathRes = getPath(targetID, &targetPath);
         if(unlikely(!getPathRes) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to remove last buddy comm override for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            return;
         }

         overrideFilename = targetPath + "/" LAST_BUDDY_COMM_OVERRIDE_FILENAME;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         int unlinkRes = unlink(overrideFilename.c_str());
         int errCode = errno;
         if ((unlinkRes != 0) && (errCode != ENOENT))
         { // error; note: if file doesn't exist, that's not considered an error
            LogContext(__func__).logErr(
               "Unable to unlink override file for last buddy communication; targetID: "
                  + StringTk::uintToStr(targetID) + "; SysErr: " + System::getErrString(errCode));
         }

         safeLock.unlock(); // U N L O C K
      }

      void setResyncInProgress(uint16_t targetID, bool newValue)
      {
         bool currentValue = false;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         StorageTargetDataMapIter iter = this->storageTargetDataMap.find(targetID);
         if(likely(iter != this->storageTargetDataMap.end() ) )
         {
            currentValue = (iter->second).buddyResyncInProgress;

            if(newValue && !currentValue)
            { // got a new resync started => increase counter
               (iter->second).buddyResyncInProgress = newValue;
               buddyResyncCount.increase();
            }
            else
            if(!newValue && currentValue)
            { // resync finished => decrease counter
               (iter->second).buddyResyncInProgress = newValue;
               buddyResyncCount.decrease();
            }
         }
         else
            LogContext(__func__).log(Log_WARNING,
               "Tried to set resync status for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) + "; "
               "new status: " + StringTk::uintToStr(newValue) );


         safeLock.unlock(); // U N L O C K
      }

      bool isBuddyResyncInProgress(uint16_t targetID)
      {
         bool retVal;

         if (buddyResyncCount.read() == 0)
            return false;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         StorageTargetDataMapCIter iter = storageTargetDataMap.find(targetID);
         if(likely(iter != storageTargetDataMap.end() ) )
         { // found it
            retVal = (iter->second).buddyResyncInProgress;
         }
         else
         { // target not found
            LogContext(__func__).log(Log_WARNING,
               "Tried to get resync status for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            retVal = false;
         }

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * @param needsResync true to set marker, false to remove it
       */
      void setBuddyNeedsResync(uint16_t targetID, bool needsResync, uint16_t buddyTargetID)
      {
         if(needsResync)
         {
            bool fileCreated;

            createBuddyNeedsResyncFile(targetID, &fileCreated, buddyTargetID);

            if(fileCreated) // only notify mgmtd once
               setBuddyNeedsResyncState(targetID, true, false);
         }
         else
         {
            removeBuddyNeedsResyncFile(targetID);
            setBuddyNeedsResyncState(targetID, false, false);
         }
      }

      bool getBuddyNeedsResync(uint16_t targetID)
      {
         std::string path;

         bool getPathRes = getPath(targetID, &path);
         if(unlikely(!getPathRes) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to get resync needed status for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            return false;
         }

         path += "/" BUDDY_NEEDS_RESYNC_FILENAME;

         return StorageTk::pathExists(path);
      }

      /**
       * If the path to the target is already known this method can be used. It does not need to
       * access the storageTargetDataMap to get the path.
       * @returns whether the "buddyneedsresync" marker is set for a target.
       */
      bool getBuddyNeedsResyncByPath(const std::string& targetPath)
      {
         const std::string filename = targetPath + "/" BUDDY_NEEDS_RESYNC_FILENAME;

         return StorageTk::pathExists(filename);
      }

      TargetOfflineWait& getTargetOfflineWait()
      {
         return this->targetOfflineWait;
      }


      QuotaInodeSupport getSupportForInodeQuota()
      {
         return supportForInodeQuota;
      }


   private:
      bool removeAllTargetDirs()
      {
         return removeTargetDir("", true);
      }

      /**
       * @param fileCreated true if file didn't exist before and was actually created (may be NULL
       *    if caller is not interested).
       */
      void createBuddyNeedsResyncFile(uint16_t targetID, bool* fileCreated, uint16_t buddyTargetID)
      {
         const char* logContext = "StorageTargets (mark buddy resync)";

         std::string path;

         SAFE_ASSIGN(fileCreated, false);

         bool getPathRes = getPath(targetID, &path);
         if(unlikely(!getPathRes) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to set buddy resync needed for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            return;
         }

         path += "/" BUDDY_NEEDS_RESYNC_FILENAME;

         int createErrno = 0; // silence gcc
         bool createRes = StorageTk::createFile(path, &createErrno, fileCreated);

         if(unlikely(!createRes) )
         { // error
            LogContext(logContext).logErr(
               "Unable to create file for needed buddy resync. targetID: "
                  + StringTk::uintToStr(targetID) + "; fileName: " + path + "; SysErr: "
                  + System::getErrString(createErrno) );

            return;
         }

         if (*fileCreated)
         {
            // mark wasn't set before, inform user about new mark
            LOG(CRITICAL, "Secondary mirror buddy needs to be resynced.",
                  as("primaryTargetID", targetID), buddyTargetID);
         }
      }

      /**
       * @param targetID
       * @param outIsOverride set to true if an override file exists; can be NULL if not needed
       *
       * @return the lastbuddycomm timestamp, 0 if file is not found or empty.
       */
      int64_t readLastBuddyCommTimestampUnlocked(const std::string& targetPath, bool* outIsOverride)
      {
         std::string filename;
         // if an override file exists, use that
         if (StorageTk::pathExists(targetPath + "/" + LAST_BUDDY_COMM_OVERRIDE_FILENAME))
         {
            filename = targetPath + "/" + LAST_BUDDY_COMM_OVERRIDE_FILENAME;
            if (outIsOverride)
               *outIsOverride = true;
         }
         else
         {
            filename = targetPath + "/" + LAST_BUDDY_COMM_TIMESTAMP_FILENAME;
            if (outIsOverride)
               *outIsOverride = false;
         }

         // check if file exists and read it
         bool pathExists = StorageTk::pathExists(filename);
         if(!pathExists)
            return 0;

         StringList fileContentsList; // actually, the file would contain only a single line
         ICommonConfig::loadStringListFile(filename.c_str(), fileContentsList);

         if(fileContentsList.empty() )
            return 0;

         return StringTk::strToInt64(fileContentsList.front());
      }

      void removeBuddyNeedsResyncFile(uint16_t targetID)
      {
         std::string path;

         bool getPathRes = getPath(targetID, &path);
         if(unlikely(!getPathRes) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to remove buddy resync needed for unknown target. "
               "targetID: " + StringTk::uintToStr(targetID) );
            return;
         }

         path += "/" BUDDY_NEEDS_RESYNC_FILENAME;

         int unlinkRes = unlink(path.c_str() );

         if(unlikely( (unlinkRes != 0) && (errno != ENOENT) ) )
         { // error
            LogContext(__func__).logErr(
               "Unable to remove file for needed buddy resync. targetID: "
                  + StringTk::uintToStr(targetID) + "; filename: " + path + "; SysErr: "
                  + System::getErrString(errno) );
         }
      }
};


#endif /* STORAGETARGETS_H_ */

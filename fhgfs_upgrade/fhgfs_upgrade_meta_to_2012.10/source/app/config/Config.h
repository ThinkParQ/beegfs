#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>


enum TargetChooserType
{
   TargetChooserType_RANDOMIZED = 0,
   TargetChooserType_ROUNDROBIN = 1,
   TargetChooserType_RANDOMROBIN = 2 // randomized round-robin (round-robin result list is shuffled)
};


class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv) throw(InvalidConfigException);
   
   private:

      // configurables

      std::string       connInterfacesFile; // implicitly generates connInterfacesList
      std::string       connInterfacesList; // comma-separated list

      bool              debugRunComponentThreads;
      bool              debugRunStartupTests;
      bool              debugRunComponentTests;
      bool              debugRunIntegrationTests;
      
      std::string       storeMetaDirectory;
      bool              storeAllowFirstRunInit;
      bool              storeUseExtendedAttribs;
      bool              storeSelfHealEmptyFiles;
      
      unsigned          tuneNumWorkers; // 0 means automatic
      unsigned          tuneWorkerBufSize;
      unsigned          tuneNumCommSlaves; // 0 means automatic
      unsigned          tuneCommSlaveBufSize;
      unsigned          tuneDefaultChunkSize;
      unsigned          tuneDefaultNumStripeTargets;
      unsigned          tuneProcessFDLimit; // 0 means "don't touch limit"
      bool              tuneWorkerNumaAffinity;
      bool              tuneListenerNumaAffinity;
      int               tuneListenerPrioShift; // inc/dec thread priority of listener components
      unsigned          tuneDirMetadataCacheLimit;
      std::string       tuneTargetChooser;
      TargetChooserType tuneTargetChooserNum;  // auto-generated based on tuneTargetChooser
      unsigned          tuneLockGrantWaitMS; // time to wait for an ack per retry
      unsigned          tuneLockGrantNumRetries; // number of lock grant send retries until ack recv
      bool              tuneRotateMirrorTargets; // true to use rotated targets list as mirrors
      
      bool              runDaemonized;

      std::string       pidFile;

      // upgrade tool options
      std::string       targetIdMapFile;
      std::string       metaIdMapFile;
      bool              deleteOldFiles;

      
      // internals

      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes)
         throw(InvalidConfigException);
      virtual void initImplicitVals() throw(InvalidConfigException);
      void initTuneTargetChooserNum() throw(InvalidConfigException);
      std::string createDefaultCfgFilename();


      
   public:
      // getters & setters

      std::string getConnInterfacesList() const
      {
         return connInterfacesList;
      }

      bool getDebugRunComponentThreads() const
      {
         return debugRunComponentThreads;
      }

      bool getDebugRunStartupTests() const
      {
         return debugRunStartupTests;
      }

      bool getDebugRunComponentTests() const
      {
         return debugRunComponentTests;
      }

      bool getDebugRunIntegrationTests() const
      {
         return debugRunIntegrationTests;
      }
      
      std::string getStoreMetaDirectory() const
      {
         return storeMetaDirectory;
      }
      
      bool getStoreAllowFirstRunInit() const
      {
         return storeAllowFirstRunInit;
      }

      bool getStoreUseExtendedAttribs() const
      {
         return storeUseExtendedAttribs;
      }

      void setStoreUseExtendedAttribs(bool value)
      {
         this->storeUseExtendedAttribs = value;
      }

      bool getStoreSelfHealEmptyFiles() const
      {
         return storeSelfHealEmptyFiles;
      }

      unsigned getTuneNumWorkers() const
      {
         return tuneNumWorkers;
      }

      unsigned getTuneWorkerBufSize() const
      {
         return tuneWorkerBufSize;
      }

      unsigned getTuneNumCommSlaves() const
      {
         return tuneNumCommSlaves;
      }

      unsigned getTuneCommSlaveBufSize() const
      {
         return tuneCommSlaveBufSize;
      }

      unsigned getTuneDefaultChunkSize() const
      {
         return tuneDefaultChunkSize;
      }
      
      unsigned getTuneDefaultNumStripeTargets() const
      {
         return tuneDefaultNumStripeTargets;
      }

      unsigned getTuneProcessFDLimit() const
      {
         return tuneProcessFDLimit;
      }

      bool getTuneWorkerNumaAffinity() const
      {
         return tuneWorkerNumaAffinity;
      }

      bool getTuneListenerNumaAffinity() const
      {
         return tuneListenerNumaAffinity;
      }

      int getTuneListenerPrioShift() const
      {
         return tuneListenerPrioShift;
      }
      
      unsigned getTuneDirMetadataCacheLimit() const
      {
         return tuneDirMetadataCacheLimit;
      }

      TargetChooserType getTuneTargetChooserNum() const
      {
         return tuneTargetChooserNum;
      }

      unsigned getTuneLockGrantWaitMS() const
      {
         return tuneLockGrantWaitMS;
      }

      unsigned getTuneLockGrantNumRetries() const
      {
         return tuneLockGrantNumRetries;
      }

      bool getTuneRotateMirrorTargets() const
      {
         return tuneRotateMirrorTargets;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      std::string getPIDFile() const
      {
         return pidFile;
      }

      std::string getTargetIdMapFile() const
      {
         return targetIdMapFile;
      }

      std::string getMetaIdMapFile() const
      {
         return metaIdMapFile;
      }

      bool getDeleteOldFiles()
      {
         return deleteOldFiles;
      }

      std::string getLogStdFile() const
      {
         return logStdFile;
      }
};

#endif /*CONFIG_H_*/

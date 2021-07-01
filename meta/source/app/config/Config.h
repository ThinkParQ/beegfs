#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>
#include <common/nodes/TargetCapacityPools.h>


enum TargetChooserType
{
   TargetChooserType_RANDOMIZED = 0,
   TargetChooserType_ROUNDROBIN = 1, // round-robin in ID order
   TargetChooserType_RANDOMROBIN = 2, // randomized round-robin (round-robin, but shuffle result)
   TargetChooserType_RANDOMINTERNODE = 3, // select random targets from different nodes/domains
   TargetChooserType_RANDOMINTRANODE = 4, // select random targets from the same node/domain
};


class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);
      virtual ~Config();

   private:

      // configurables

      std::string       connInterfacesFile; // implicitly generates connInterfacesList
      std::string       connInterfacesList; // comma-separated list

      std::string       storeMetaDirectory;
      std::string       storeFsUUID;
      bool              storeAllowFirstRunInit;
      bool              storeUseExtendedAttribs;
      bool              storeSelfHealEmptyFiles;

      bool              storeClientXAttrs;
      bool              storeClientACLs;

      std::string       sysTargetAttachmentFile; // used by randominternode target chooser
      TargetMap*        sysTargetAttachmentMap; /* implicitly by sysTargetAttachmentFile, NULL if
                                                   unset */

      unsigned          tuneNumStreamListeners;
      unsigned          tuneNumWorkers; // 0 means automatic
      unsigned          tuneWorkerBufSize;
      unsigned          tuneNumCommSlaves; // 0 means automatic
      unsigned          tuneCommSlaveBufSize;
      unsigned          tuneDefaultChunkSize;
      unsigned          tuneDefaultNumStripeTargets;
      unsigned          tuneProcessFDLimit; // 0 means "don't touch limit"
      bool              tuneWorkerNumaAffinity;
      bool              tuneListenerNumaAffinity;
      int               tuneBindToNumaZone; // bind all threads to this zone, -1 means no binding
      int               tuneListenerPrioShift; // inc/dec thread priority of listener components
      unsigned          tuneDirMetadataCacheLimit;
      std::string       tuneTargetChooser;
      TargetChooserType tuneTargetChooserNum;  // auto-generated based on tuneTargetChooser
      unsigned          tuneLockGrantWaitMS; // time to wait for an ack per retry
      unsigned          tuneLockGrantNumRetries; // number of lock grant send retries until ack recv
      bool              tuneRotateMirrorTargets; // true to use rotated targets list as mirrors
      bool              tuneEarlyUnlinkResponse; // true to send response before chunk files unlink
      bool              tuneUsePerUserMsgQueues; // true to use UserWorkContainer for MultiWorkQueue
      bool              tuneUseAggressiveStreamPoll; // true to not sleep on epoll in streamlisv2
      unsigned          tuneNumResyncSlaves;
      bool              tuneMirrorTimestamps;
      unsigned          tuneDisposalGCPeriod; // sleep between disposal garbage collector runs [seconds], 0 = disabled

      bool              quotaEarlyChownResponse; // true to send response before chunk files chown
      bool              quotaEnableEnforcement;

      unsigned          sysTargetOfflineTimeoutSecs;
      bool              sysAllowUserSetPattern;

      bool              runDaemonized;

      std::string       pidFile;

      bool limitXAttrListLength;
      std::string sysFileEventLogTarget;


      // internals

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      void initSysTargetAttachmentMap();
      void initTuneTargetChooserNum();
      std::string createDefaultCfgFilename() const;

   public:
      // getters & setters
      const std::string& getConnInterfacesList() const
      {
         return connInterfacesList;
      }

      const std::string& getStoreMetaDirectory() const
      {
         return storeMetaDirectory;
      }

      const std::string& getStoreFsUUID() const
      {
         return storeFsUUID;
      }

      bool getStoreAllowFirstRunInit() const
      {
         return storeAllowFirstRunInit;
      }

      bool getStoreUseExtendedAttribs() const
      {
         return storeUseExtendedAttribs;
      }

      bool getStoreSelfHealEmptyFiles() const
      {
         return storeSelfHealEmptyFiles;
      }

      bool getStoreClientXAttrs() const
      {
         return storeClientXAttrs;
      }

      bool getStoreClientACLs() const
      {
         return storeClientACLs;
      }

      const std::string& getSysTargetAttachmentFile() const
      {
         return sysTargetAttachmentFile;
      }

      const TargetMap* getSysTargetAttachmentMap() const
      {
         return sysTargetAttachmentMap;
      }

      unsigned getTuneNumStreamListeners() const
      {
         return tuneNumStreamListeners;
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

      int getTuneBindToNumaZone() const
      {
         return tuneBindToNumaZone;
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

      bool getTuneEarlyUnlinkResponse() const
      {
         return tuneEarlyUnlinkResponse;
      }

      bool getTuneUsePerUserMsgQueues() const
      {
         return tuneUsePerUserMsgQueues;
      }

      bool getTuneUseAggressiveStreamPoll() const
      {
         return tuneUseAggressiveStreamPoll;
      }

      unsigned getTuneNumResyncSlaves() const
      {
         return tuneNumResyncSlaves;
      }

      bool getQuotaEarlyChownResponse() const
      {
         return quotaEarlyChownResponse;
      }

      bool getQuotaEnableEnforcement() const
      {
         return quotaEnableEnforcement;
      }

      void setQuotaEnableEnforcement(bool doQuotaEnforcement)
      {
         quotaEnableEnforcement = doQuotaEnforcement;
      }

      unsigned getSysTargetOfflineTimeoutSecs() const
      {
         return sysTargetOfflineTimeoutSecs;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      const std::string& getPIDFile() const
      {
         return pidFile;
      }

      bool getTuneMirrorTimestamps() const { return tuneMirrorTimestamps; }

      unsigned getTuneDisposalGCPeriod() const { return tuneDisposalGCPeriod; }

      bool getSysAllowUserSetPattern() const { return sysAllowUserSetPattern; }

      bool getLimitXAttrListLength() const { return limitXAttrListLength; }

      void setLimitXAttrListLength(bool value) { limitXAttrListLength = value; }

      const std::string& getFileEventLogTarget() const { return sysFileEventLogTarget; }
};

#endif /*CONFIG_H_*/

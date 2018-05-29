#ifndef CONFIG_H_
#define CONFIG_H_


#include <common/cache/app/config/CacheConfig.h>
#include <common/Common.h>


class Config : public CacheConfig
{
   public:
      Config(int argc, char** argv);

   private:

      // configurables
      unsigned    tuneNumStreamListeners;
      unsigned    tuneNumWorkers; // 0 means automatic
      unsigned    tuneWorkerBufSize;
      unsigned    tuneNumCommSlaves; // 0 means automatic
      unsigned    tuneCommSlaveBufSize;
      bool        tuneWorkerNumaAffinity;
      bool        tuneListenerNumaAffinity;
      int         tuneBindToNumaZone; // bind all threads to this zone, -1 means no binding
      int         tuneListenerPrioShift; // inc/dec thread priority of listener components
      bool        tuneUseAggressiveStreamPoll; // true to not sleep on epoll in streamlisv2
      uint64_t    tuneFileSplitSize;

      bool        runDaemonized;

      uint64_t    logHealthCheckIntervalSec;

      std::string pidFile;


      // internals

      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes);
      virtual void initImplicitVals();
      std::string createDefaultCfgFilename() const;


   public:
      // getters & setters

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

      bool getTuneUseAggressiveStreamPoll() const
      {
         return tuneUseAggressiveStreamPoll;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      const std::string& getPIDFile() const
      {
         return pidFile;
      }

      uint64_t getTuneFileSplitSize() const
      {
         return tuneFileSplitSize;
      }

      uint64_t getLogHealthCheckIntervalSec() const
      {
         return logHealthCheckIntervalSec;
      }
};

#endif /*CONFIG_H_*/

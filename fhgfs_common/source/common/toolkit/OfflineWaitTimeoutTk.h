#ifndef OFFLINEWAITTIMEOUTTK_H_
#define OFFLINEWAITTIMEOUTTK_H_

/**
 * Calculates the offline wait timeout from config variables. It consists of:
 *  5 sec InternodeSyncer syncLoop interval.
 *  3 * update interval:
 *      until target gets pofflined (2x)
 *      end of offline timeout until next target state update (worst case)
 *  plus the actual target offline timeout.
 *
 * Templated for the Config type because Storage and Meta server have different Config classes.
 */
template<typename Cfg>
class OfflineWaitTimeoutTk
{
public:
   static unsigned int calculate(Cfg* cfg)
   {
      const unsigned updateTargetStatesSecs = cfg->getSysUpdateTargetStatesSecs();

      if (updateTargetStatesSecs != 0)
      {
         // If sysUpdateTargetStateSecs is set in config, use that value.
         return (
            ( 5
              + 3 * updateTargetStatesSecs
              + cfg->getSysTargetOfflineTimeoutSecs()
            ) * 1000);
      }
      else
      {
         // If sysUpdateTargetStatesSecs hasn't been set in config, it defaults to 1/3 the value
         // of sysTargetOfflineTimeoutSecs -> we use 3 * 1/3 sysTargetOfflineTimeoutSecs.
         return (
            ( 5
              + 2 * cfg->getSysTargetOfflineTimeoutSecs()
            ) * 1000);
      }
   }
};

#endif /* OFFLINEWAITTIMEOUTTK_H_ */

#ifndef CLIENTSTATSOPTIONS_H_
#define CLIENTSTATSOPTIONS_H_

#include <common/nodes/Node.h>

#define MODECLIENTSTATS_ARG_INTERVALSECS     "--interval"
#define MODECLIENTSTATS_ARG_MAXLINES         "--maxlines"
#define MODECLIENTSTATS_ARG_ALLSTATS         "--allstats"
#define MODECLIENTSTATS_ARG_RWUNIT           "--rwunit"
#define MODECLIENTSTATS_ARG_PERINTERVAL      "--perinterval"
#define MODECLIENTSTATS_ARG_SHOWNAMES        "--names"
#define MODECLIENTSTATS_ARG_FILTER           "--filter"


/**
 * Stores user options of ModeClientStats.
 */
class CfgClientStatsOptions
{
   public:
      enum NodeType nodeType;   // the requested node type;
      unsigned maxLines;        // maximum number of lines/clients to print
      bool allStats;            // do not suppress zero values in stats output
      unsigned intervalSecs;    // interval to get and print stats
      std::string unit;         // unit for read- and write-bytes
      std::string nodeID;       // do stats for the given nodeID only
      bool perInterval;         // stat values per interval and not per second
      bool perUser;             /* show per-user statistics, not per-client; in this mode, we will
                                   get userIDs from the servers instead of clientIPs */
      bool showNames;           // show hostnames/usernames instead of IPs/userIDs
      std::string filter;       // if set, show values for this client/user only

      /**
       * Constructor used by admon
       *
       * @param intervalSecs interval to get and print stats
       * @param maxLines maximum number of lines/clients to print/request
       * @param nodeType the requested node type
       * @param perUser if true user stats are requested, if false client stats are requested
       *
       */
      CfgClientStatsOptions(unsigned intervalSecs, unsigned maxLines, NodeType nodeType,
         bool perUser)
      {
         this->intervalSecs = intervalSecs;     // every 10s by default
         this->maxLines     = maxLines;
         this->allStats     = true;             // dynamic mode by default
         this->unit         = "MiB";            // MiB by default
         this->nodeID       = "";
         this->perInterval  = false;
         this->perUser      = perUser;
         this->showNames    = false;
         this->nodeType     = nodeType;
      }

      /**
       * Default constructor.
       */
      CfgClientStatsOptions()
      {
         this->intervalSecs = 10;    // every 10s by default
         this->maxLines     = 20;    // top 20 lines/clients by default
         this->allStats     = false; // dynamic mode by default
         this->unit         = "MiB"; // MiB by default
         this->nodeID       = "";
         this->perInterval  = false;
         this->perUser      = false;
         this->showNames    = false;

         this->nodeType     = NODETYPE_Invalid;
      }

      bool isEqualForAdmon(unsigned intervalSecs, unsigned maxLines, NodeType nodeType,
         bool doUserStats)
      {
         if (this->intervalSecs != intervalSecs)
            return false;
         if (this->maxLines != maxLines)
            return false;
         if (this->nodeType != nodeType)
            return false;
         if (this->perUser != doUserStats)
            return false;

         return true;
      }

      int getParams(StringMap* cfg);
};

#endif /* CLIENTSTATSOPTIONS_H_ */

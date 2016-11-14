#ifndef ICOMMONCONFIG_H_
#define ICOMMONCONFIG_H_

#include <common/Common.h>
#include "InvalidConfigException.h"

class ICommonConfig
{
   public:
      virtual ~ICommonConfig() {}

      static void loadStringListFile(const char* filename, StringList& outList)
         throw(InvalidConfigException);
      
   protected:
      ICommonConfig() {}

      int         logLevel;
      bool        logErrsToStdlog;
      bool        logNoDate;
      std::string logStdFile;
      std::string logErrFile;
      unsigned    logNumLines;
      unsigned    logNumRotatedFiles;

      int         connPortShift; // shifts all UDP and TCP ports
      int         connClientPortUDP;
      int         connStoragePortUDP;
      int         connMetaPortUDP;
      int         connAdmonPortUDP;
      int         connMgmtdPortUDP;
      int         connStoragePortTCP;
      int         connMetaPortTCP;
      int         connHelperdPortTCP;
      int         connMgmtdPortTCP;
      bool        connUseSDP;
      bool        connUseRDMA;
      unsigned    connBacklogTCP;
      unsigned    connMaxInternodeNum;
      unsigned    connFallbackExpirationSecs;
      unsigned    connRDMABufSize;
      unsigned    connRDMABufNum;
      uint8_t     connRDMATypeOfService;
      std::string connNetFilterFile; // for allowed IPs (empty means "allow all")
      std::string connAuthFile;
      uint64_t    connAuthHash; // implicitly set based on hash of connAuthFile contents
      std::string connTcpOnlyFilterFile; // for IPs that only allow plain TCP (no RDMA etc)

      bool        debugFindOtherNodes;

      std::string sysMgmtdHost;
      unsigned    sysUpdateTargetStatesSecs;

      bool runUnitTests;
      std::string testOutputFile;
      std::string testOutputFormat; // plain/xml


   public:
      // getters & setters
      int getLogLevel() const
      {
         return logLevel;
      }
      
      bool getLogErrsToStdlog() const
      {
         return logErrsToStdlog;
      }
      
      bool getLogNoDate() const
      {
         return logNoDate;
      }
      
      std::string getLogStdFile() const
      {
         return logStdFile;
      }
      
      std::string getLogErrFile() const
      {
         return logErrFile;
      }
      
      unsigned getLogNumLines() const
      {
         return logNumLines;
      }
      
      unsigned getLogNumRotatedFiles() const
      {
         return logNumRotatedFiles;
      }

      int getConnClientPortUDP() const
      {
         return connClientPortUDP ? (connClientPortUDP + connPortShift) : 0;
      }

      int getConnStoragePortUDP() const
      {
         return connStoragePortUDP ? (connStoragePortUDP + connPortShift) : 0;
      }

      int getConnMetaPortUDP() const
      {
         return connMetaPortUDP ? (connMetaPortUDP + connPortShift) : 0;
      }
      
      int getConnAdmonPortUDP() const
      {
         return connAdmonPortUDP ? (connAdmonPortUDP + connPortShift) : 0;
      }

      int getConnMgmtdPortUDP() const
      {
         return connMgmtdPortUDP ? (connMgmtdPortUDP + connPortShift) : 0;
      }

      int getConnStoragePortTCP() const
      {
         return connStoragePortTCP ? (connStoragePortTCP + connPortShift) : 0;
      }
      
      int getConnMetaPortTCP() const
      {
         return connMetaPortTCP ? (connMetaPortTCP + connPortShift) : 0;
      }

      int getConnHelperdPortTCP() const
      {
         return connHelperdPortTCP ? (connHelperdPortTCP + connPortShift) : 0;
      }

      int getConnMgmtdPortTCP() const
      {
         return connMgmtdPortTCP ? (connMgmtdPortTCP + connPortShift) : 0;
      }
      
      bool getConnUseSDP() const
      {
         return connUseSDP;
      }

      bool getConnUseRDMA() const
      {
         return connUseRDMA;
      }
      
      unsigned getConnBacklogTCP() const
      {
         return connBacklogTCP;
      }
      
      unsigned getConnMaxInternodeNum() const
      {
         return connMaxInternodeNum;
      }

      int getConnFallbackExpirationSecs() const
      {
         return connFallbackExpirationSecs;
      }
      
      unsigned getConnRDMABufSize() const
      {
         return connRDMABufSize;
      }

      unsigned getConnRDMABufNum() const
      {
         return connRDMABufNum;
      }
      
      uint8_t getConnRDMATypeOfService() const
      {
         return connRDMATypeOfService;
      }

      std::string getConnNetFilterFile() const
      {
         return connNetFilterFile;
      }
      
      std::string getConnAuthFile() const
      {
         return connAuthFile;
      }

      uint64_t getConnAuthHash() const
      {
         return connAuthHash;
      }

      std::string getConnTcpOnlyFilterFile() const
      {
         return connTcpOnlyFilterFile;
      }

      bool getDebugFindOtherNodes() const
      {
         return debugFindOtherNodes;
      }

      std::string getSysMgmtdHost() const
      {
         return sysMgmtdHost;
      }

      unsigned getSysUpdateTargetStatesSecs() const
      {
         return sysUpdateTargetStatesSecs;
      }

      bool getRunUnitTests() const
      {
         return runUnitTests;
      }

      std::string getTestOutputFile() const
      {
         return testOutputFile;
      }

      std::string getTestOutputFormat() const
      {
         return testOutputFormat;
      }
};

#endif /*ICOMMONCONFIG_H_*/

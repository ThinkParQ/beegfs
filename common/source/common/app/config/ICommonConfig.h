#pragma once

#include <common/Common.h>
#include "InvalidConfigException.h"

class ICommonConfig
{
   public:
      virtual ~ICommonConfig() {}

      static void loadStringListFile(const char* filename, StringList& outList);

   protected:
      ICommonConfig() {}

      LogType     logType; 
      int         logLevel;
      bool        logNoDate;
      std::string logStdFile;
      unsigned    logNumLines;
      unsigned    logNumRotatedFiles;

      int         connPortShift; // shifts all UDP and TCP ports
      int         connClientPort;
      int         connStoragePort;
      int         connMetaPort;
      int         connMonPort;
      int         connMgmtdPort;
      bool        connUseRDMA;
      unsigned    connBacklogTCP;
      unsigned    connMaxInternodeNum;
      unsigned    connFallbackExpirationSecs;
      int         connTCPRcvBufSize;
      int         connUDPRcvBufSize;
      unsigned    connRDMABufSize;
      unsigned    connRDMABufNum;
      uint8_t     connRDMATypeOfService;
      std::string connNetFilterFile; // for allowed IPs (empty means "allow all")
      std::string connAuthFile;
      bool        connDisableAuthentication;
      uint64_t    connAuthHash; // implicitly set based on hash of connAuthFile contents
      std::string connTcpOnlyFilterFile; // for IPs that only allow plain TCP (no RDMA etc)
      bool        connRestrictOutboundInterfaces;
      std::string connNoDefaultRoute;

      int         connMsgLongTimeout;
      int         connMsgMediumTimeout;
      int         connMsgShortTimeout; // connection (response) timeouts in ms
                                       // note: be careful here, because servers not
                                       // responding for >30secs under high load is nothing
                                       // unusual, so never use connMsgShortTimeout for
                                       // IO-related operations.
      int         connRDMATimeoutConnect;
      int         connRDMATimeoutFlowSend;
      int         connRDMATimeoutPoll;

      std::string sysMgmtdHost;
      unsigned    sysUpdateTargetStatesSecs;

      int         connectionRejectionRate;
      int         connectionRejectionCount;


   public:
      // getters & setters
      LogType getLogType() const
      {
         return logType;
      }
      
      int getLogLevel() const
      {
         return logLevel;
      }

      bool getLogNoDate() const
      {
         return logNoDate;
      }

      const std::string& getLogStdFile() const
      {
         return logStdFile;
      }

      unsigned getLogNumLines() const
      {
         return logNumLines;
      }

      unsigned getLogNumRotatedFiles() const
      {
         return logNumRotatedFiles;
      }

      int getConnClientPort() const
      {
         return connClientPort ? (connClientPort + connPortShift) : 0;
      }

      int getConnStoragePort() const
      {
         return connStoragePort ? (connStoragePort + connPortShift) : 0;
      }

      int getConnMetaPort() const
      {
         return connMetaPort ? (connMetaPort + connPortShift) : 0;
      }

      int getConnMonPort() const
      {
         return connMonPort ? (connMonPort + connPortShift) : 0;
      }

      int getConnMgmtdPort() const
      {
         return connMgmtdPort ? (connMgmtdPort + connPortShift) : 0;
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

      int getConnTCPRcvBufSize() const
      {
         return connTCPRcvBufSize;
      }

      int getConnUDPRcvBufSize() const
      {
         return connUDPRcvBufSize;
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

      const std::string& getConnNetFilterFile() const
      {
         return connNetFilterFile;
      }

      const std::string& getConnAuthFile() const
      {
         return connAuthFile;
      }

      uint64_t getConnAuthHash() const
      {
         return connAuthHash;
      }

      const std::string& getConnTcpOnlyFilterFile() const
      {
         return connTcpOnlyFilterFile;
      }

      bool getConnRestrictOutboundInterfaces() const
      {
         return connRestrictOutboundInterfaces;
      }

      std::string getConnNoDefaultRoute() const
      {
         return connNoDefaultRoute;
      }

      int getConnMsgLongTimeout() const
      {
        return connMsgLongTimeout;
      }

      int getConnMsgMediumTimeout() const
      {
        return connMsgMediumTimeout;
      }

      int getConnMsgShortTimeout() const
      {
        return connMsgShortTimeout;
      }

      int getConnRDMATimeoutConnect() const
      {
        return connRDMATimeoutConnect;
      }

      int getConnRDMATimeoutFlowSend() const
      {
        return connRDMATimeoutFlowSend;
      }

      int getConnRDMATimeoutPoll() const
      {
        return connRDMATimeoutPoll;
      }

      const std::string& getSysMgmtdHost() const
      {
         return sysMgmtdHost;
      }

      unsigned getSysUpdateTargetStatesSecs() const
      {
         return sysUpdateTargetStatesSecs;
      }

      unsigned getConnectionRejectionRate() const
      {
         return connectionRejectionRate;
      }

      void setConnectionRejectionRate(unsigned rate)
      {
         connectionRejectionRate = rate;
         connectionRejectionCount = 0;
      }
};


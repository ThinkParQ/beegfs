#ifndef TOOLKIT_MANAGEMENTHELPER_H_
#define TOOLKIT_MANAGEMENTHELPER_H_


#include <common/NumericID.h>
#include <toolkit/management/FileEntry.h>
#include <toolkit/stats/statshelper.h>



namespace managementhelper
{
   std::string formatDateSec(uint64_t sec);
   std::string formatFileMode(int mode);

   bool getEntryInfo(std::string pathStr, std::string *outChunkSize,
      std::string *outNumTargets, UInt16Vector *outPrimaryTargetNumIDs, unsigned* outPatternID,
      UInt16Vector *outSecondaryTargetNumIDs, UInt16Vector *outStorageBMGs,
      NumNodeID* outPrimaryMetaNodeNumID, NumNodeID* outSecondaryMetaNodeNumID,
      uint16_t* outMetaBMG);
   bool setPattern(std::string pathStr, uint chunkSize, uint defaultNumNodes,
      unsigned patternID, bool doMetaMirroring);
   int listDirFromOffset(std::string pathStr, int64_t *offset, FileEntryList *outEntries,
      uint64_t *tmpTotalSize, short count);

   int file2HTML(std::string filename, std::string *outStr);
   int getFileContents(std::string filename, std::string *outStr);
   int getLogFile(NodeType nodeType, uint16_t nodeNumID, uint lines, std::string *outStr,
      std::string nodeID);

   void restartAdmon();
   int startService(std::string service, StringList *failedNodes);
   int startService(std::string service, std::string host);
   int stopService(std::string service, StringList *failedNodes);
   int stopService(std::string service, std::string host);
   int checkStatus(std::string service, std::string host, std::string *status);
   int checkStatus(std::string service, StringList *runningHosts, StringList *downHosts);
} /* namespace managementhelper */

#endif /* TOOLKIT_MANAGEMENT_MANAGEMENTHELPER_H_ */

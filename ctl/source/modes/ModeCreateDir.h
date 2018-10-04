#ifndef MODECREATEDIR_H_
#define MODECREATEDIR_H_

#include <common/Common.h>
#include "Mode.h"


class ModeCreateDir : public Mode
{
   private:
      struct DirSettings
      {
         unsigned userID;
         unsigned groupID;
         int mode;
         bool noMirroring;
         UInt16List* preferredNodes;
         Path* path;
         bool useMountedPath;
      };


   public:
      ModeCreateDir() {}

      virtual int execute();

      static void printHelp();


   protected:


   private:
      bool initDirSettings(DirSettings* settings);
      void freeDirSettings(DirSettings* settings);
      std::string generateServerPath(DirSettings* settings, std::string entryID);
      bool communicate(Node& ownerNode, EntryInfo* parentInfo, DirSettings* settings);

};

#endif /* MODECREATEDIR_H_ */

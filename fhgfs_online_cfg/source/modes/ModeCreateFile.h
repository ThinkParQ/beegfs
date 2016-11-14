#ifndef MODECREATEFILE_H_
#define MODECREATEFILE_H_

#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>
#include "Mode.h"


class ModeCreateFile : public Mode
{
   private:
      struct FileSettings
      {
         unsigned userID;
         unsigned groupID;
         int mode;
         UInt16List* preferredTargets;
         StripePattern* pattern;
         Path* path;
      };


   public:
      virtual int execute();

      static void printHelp();



   private:
      bool initFileSettings(FileSettings* settings);
      void freeFileSettings(FileSettings* settings);
      std::string generateServerPath(FileSettings* settings, std::string entryID);
      bool communicate(Node& ownerNode, EntryInfo* entryInfo, FileSettings* settings);
      bool communicateWithoutPattern(Node& ownerNode, std::string entryID, FileSettings* settings);
      bool communicateWithPattern(Node& ownerNode, EntryInfo* entryInfo, FileSettings* settings);
};

#endif /* MODECREATEFILE_H_ */

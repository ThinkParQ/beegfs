#ifndef MODEFIND_H_
#define MODEFIND_H_

/*
 * Find certain types of files. The user specifies which file to find (e.g. given by the storage
 * cfgTargetID), we walk through the local (client fhgfs-mounted) filesystem, find all files and
 * then check by querying meta information of the file matches.
 */


#include <program/Program.h>
#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>
#include <modes/Mode.h>
#include <common/toolkit/MetadataTk.h>
#include "ModeMigrate.h"


class ModeFind : public Mode
{
   public:

      /**
       * Entry method for ModeFind
       */
      virtual int execute()
      {
         ModeMigrate mode;
         return mode.executeFind();
      }

      static void printHelp(void)
      {
         ModeMigrate::printHelpFind();
      }
};

#endif /*MODEFIND_H_*/

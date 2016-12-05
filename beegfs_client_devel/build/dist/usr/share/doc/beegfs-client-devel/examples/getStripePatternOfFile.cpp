#include <beegfs/beegfs.h>

#include <errno.h>
#include <iostream>
#include <stdlib.h>



static const mode_t MODE_FLAG = S_IRWXU | S_IRGRP | S_IROTH;
static const int OPEN_FLAGS = O_RDWR;


int main(int argc, char** argv)
{
   // check if a path to the file is provided
   if(argc != 2)
   {
      std::cout << "Usage: " << argv[0] << " $PATH_TO_FILE" << std::endl;
      exit(-1);
   }

   std::string file(argv[1]);

   // open the provided file
   int fd = open(file.c_str(), OPEN_FLAGS, MODE_FLAG);
   if(fd == -1)
   {
      std::cout << "Open: can not open file: " << file << " errno: " << errno << std::endl;
      exit(-1);
   }

   // check if the file is located on a BeeGFS, because the striping API works only on a BeeGFS
   // if the ioctl is used on a other filesystem everything can happens
   bool isBeegfs = beegfs_testIsBeeGFS(fd);
   if(!isBeegfs)
   {
      std::cout << "The given file is not located on an BeeGFS: " << file << std::endl;
      exit(-1);
   }

   unsigned outPatternType = 0;
   unsigned outChunkSize = 0;
   uint16_t outNumTargets = 0;

   // retrive the stripe pattern of the file and print them to the console
   bool stripeInfoRetVal = beegfs_getStripeInfo(fd, &outPatternType, &outChunkSize, &outNumTargets);
   if(stripeInfoRetVal)
   {
      std::string patternType;
      switch(outPatternType)
      {
         case BEEGFS_STRIPEPATTERN_RAID0:
            patternType = "RAID0";
            break;
         case BEEGFS_STRIPEPATTERN_RAID10:
            patternType = "RAID10";
            break;
         case BEEGFS_STRIPEPATTERN_BUDDYMIRROR:
            patternType = "BUDDYMIRROR";
            break;
         default:
            patternType = "INVALID";
      }
      std::cout << "Strip pattern of file: " << file << std::endl;
      std::cout << "+ Type: " << patternType << std::endl;
      std::cout << "+ Chunksize: " << outChunkSize << " Byte" << std::endl;
      std::cout << "+ Number of storage targets: " << outNumTargets << std::endl;
      std::cout << "+ Storage targets:" << std::endl;

      // get the targets which are used for the file and print them to the console
      for (int targetIndex = 0; targetIndex < outNumTargets; targetIndex++)
      {
         struct BeegfsIoctl_GetStripeTargetV2_Arg outTargetInfo;

         bool stripeTargetRetVal = beegfs_getStripeTargetV2(fd, targetIndex, &outTargetInfo);
         if(stripeTargetRetVal)
         {
            if(outPatternType == BEEGFS_STRIPEPATTERN_BUDDYMIRROR)
            {
               std::cout << "  + " << outTargetInfo.targetOrGroup << " @ " << outTargetInfo.primaryTarget << " @ " << outTargetInfo.primaryNodeStrID << " [ID: "<< outTargetInfo.primaryNodeID << "]" << std::endl;
               std::cout << "  + " << outTargetInfo.targetOrGroup << " @ " << outTargetInfo.secondaryTarget << " @ " << outTargetInfo.secondaryNodeStrID << " [ID: "<< outTargetInfo.secondaryNodeID << "]" << std::endl;
            }
            else
            {
               std::cout << "  + " << outTargetInfo.targetOrGroup << " @ " << outTargetInfo.primaryNodeStrID << " [ID: "<< outTargetInfo.primaryNodeID << "]" << std::endl;
            }
         }
         else
         {
            std::cout << "Can not get stripe targets of file: " << file << std::endl;
            exit(-1);
         }
      }
   }
   else
   {
      std::cout << "Can not get stripe info of file: " << file << std::endl;
      exit(-1);
   }
}

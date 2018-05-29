#include <beegfs/beegfs.h>

#include <dirent.h>
#include <errno.h>
#include <iostream>
#include <libgen.h>
#include <stdlib.h>



static const mode_t MODE_FLAG = S_IRWXU | S_IRGRP | S_IROTH;
static const unsigned numtargets = 8;
static const unsigned chunksize = 1048576; // 1 Mebibyte


int main(int argc, char** argv)
{
   // check if a path to the file is provided
   if(argc != 2)
   {
      std::cout << "Usage: " << argv[0] << " $PATH_TO_FILE" << std::endl;
      exit(-1);
   }

   std::string file(argv[1]);
   std::string fileName(basename(argv[1]) );
   std::string parentDirectory(dirname(argv[1]) );

   // check if we got a file name from the given path
   if(fileName.empty() )
   {
      std::cout << "Can not get file name from given path: " << file << std::endl;
      exit(-1);
   }

   // check if we got the parent directory path from the given path
   if(parentDirectory.empty() )
   {
      std::cout << "Can not get parent directory path from given path: " << file << std::endl;
      exit(-1);
   }

   // open the directory to get a directory stream 
   DIR* parentDir = opendir(parentDirectory.c_str() );
   if(parentDir == NULL)
   {
      std::cout << "Can not get directory stream of directory: " << parentDirectory
         << " errno: " << errno << std::endl;
      exit(-1);
   }

   // get a fd of the parent directory
   int fd = dirfd(parentDir);
   if(fd == -1)
   {
      std::cout << "Can not get fd from directory: " << parentDirectory
         << " errno: " << errno << std::endl;
      exit(-1);
   }

   // check if the parent directory is located on a BeeGFS, because the striping API works only on
   // BeeGFS (Results of BeeGFS ioctls on other file systems are undefined.)
   bool isBeegfs = beegfs_testIsBeeGFS(fd);
   if(!isBeegfs)
   {
      std::cout << "The given file is not located on an BeeGFS: " << file << std::endl;
      exit(-1);
   }

   // create the file with the given stripe pattern
   bool isFileCreated = beegfs_createFile(fd, fileName.c_str(), MODE_FLAG, numtargets, chunksize);
   if(isFileCreated)
   {
      std::cout << "File successful created: " << file << std::endl;
   }
   else
   {
      std::cout << "Can not create file: " << file << " errno: " << errno << std::endl;
      exit(-1);
   }
}

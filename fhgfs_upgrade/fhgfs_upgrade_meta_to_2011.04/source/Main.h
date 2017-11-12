#ifndef MAIN_H_
#define MAIN_H_

#include "Common.h"


#ifndef FHGFS_VERSION
   #define FHGFS_VERSION   "2011.04"
#endif


extern void printUsage(std::string progName);
extern bool checkAndLockStorageDir(std::string dir);
extern bool createChunksTmpDir(std::string storageDir);
extern bool moveChunkFilesToTmpDir(std::string storageDir);
extern bool switchChunksDirToNew(std::string storageDir);
extern bool createTargetIDFile(std::string storageDir);
extern void printFinalInfo();
extern int main(int argc, char** argv);



#endif /* MAIN_H_ */

#include "QuotaDefaultLimits.h"

#include <common/app/log/Logger.h>
#include <common/storage/Path.h>
#include <common/toolkit/StorageTk.h>

bool QuotaDefaultLimits::loadFromFile()
{
   bool retVal = false;

   struct stat statBuf;
   char* buf = NULL;
   size_t readRes;

   if(!storePath.length() )
      return false;

   int fd = open(storePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      LOG(QUOTA, DEBUG, "Unable to open default limits file.", storePath, sysErr);

      return false;
   }

   int statRes = fstat(fd, &statBuf);
   if(statRes != 0)
   { // stat failed
      LOG(QUOTA, WARNING, "Unable to stat default limits file.", storePath, sysErr);

      goto err_closefile;
   }

   buf = (char*)malloc(statBuf.st_size);
   if(!buf)
   { // malloc failed
      LOG(QUOTA, WARNING, "Unable to allocate memory to read the default limits file.",
          storePath, sysErr);

      goto err_closefile;
   }

   readRes = read(fd, buf, statBuf.st_size);
   if(readRes <= 0)
   { // reading failed
      LOG(QUOTA, WARNING, "Unable to read default limits file.", storePath, sysErr);

      goto err_freebuf;
   }
   else
   { // parse contents
      unsigned outLen;

      bool deserRes = deserialize(buf, readRes, &outLen);

      if (!deserRes)
         goto err_freebuf;
   }

   retVal = true;

err_freebuf:
   free(buf);

err_closefile:
   close(fd);

   if (!retVal)
      LOG(QUOTA, ERR, "Could not deserialize limits from file.", storePath);

   return retVal;
}

bool QuotaDefaultLimits::saveToFile()
{
   bool retVal = false;

   if(!storePath.length() )
      return false;

   Path quotaDataPath(storePath);
   if(!StorageTk::createPathOnDisk(quotaDataPath, true) )
   {
      LOG(QUOTA, ERR, "Unable to create directory for quota data.", storePath);
      return false;
   }

   // create/trunc file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(storePath.c_str(), openFlags, 0666);
   if(fd == -1)
   { // error
      LOG(QUOTA, ERR, "Unable to create default quota limits file.", storePath, sysErr);

      return false;
   }

   unsigned bufLen;
   ssize_t writeRes;

   // file created => store data
   char* buf = (char*)malloc(serialLen() );
   if(!buf)
   {
      LOG(QUOTA, ERR, "Unable to allocate memory to write the default limits file.",
                storePath, sysErr);
      goto err_closefile;
   }

   bufLen = serialize(buf);
   writeRes = write(fd, buf, bufLen);
   free(buf);

   if(writeRes != (ssize_t)bufLen)
   {
      LOG(QUOTA, ERR, "Unable to allocate memory to write the default limits file.",
                storePath, sysErr);
      goto err_closefile;
   }

   retVal = true;

err_closefile:
   close(fd);

   return retVal;
}

void QuotaDefaultLimits::clearLimits()
{
   defaultUserQuotaInodes = 0;
   defaultUserQuotaSize = 0;
   defaultGroupQuotaInodes = 0;
   defaultGroupQuotaSize = 0;

   int unlinkRes = unlink(storePath.c_str());

   if ( (unlinkRes != 0) && (errno != ENOENT) )
   {
      LOG(QUOTA, WARNING, "Could not delete file", ("path", storePath), sysErr);
   }
}

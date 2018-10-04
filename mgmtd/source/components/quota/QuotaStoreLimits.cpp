/*
 * QuotaStoreLimits.cpp
 */


#include <common/app/log/LogContext.h>
#include <common/toolkit/StorageTk.h>
#include <program/Program.h>
#include "QuotaStoreLimits.h"

#include <boost/scoped_array.hpp>


/**
 * Getter for the limits of a given ID. The limits are returned in the given QuotaData. The map is
 * read locked during the execution.
 *
 * @param quotaDataInOut the input and output QuotaData for this method, the ID and the type must
 *        set before the method was called, the method updates the size and inode value of this
 *        QuotaData object with the limits for the given ID
 * @return true a limits was found
 */
bool QuotaStoreLimits::getQuotaLimit(QuotaData& quotaDataInOut)
{
#ifdef BEEGFS_DEBUG
   if (quotaDataInOut.getType() != this->quotaType)
      LogContext(this->logContext).log(Log_ERR, "Given QuotaDataType doesn't match to the "
         "QuotaDataType of the store.");
#endif

   RWLockGuard const lock(limitsRWLock, SafeRWLock_READ);

   QuotaDataMapIter iter = this->limits.find(quotaDataInOut.getID() );
   if(iter != this->limits.end() )
   {
      uint64_t size = iter->second.getSize();
      uint64_t inodes = iter->second.getInodes();
      quotaDataInOut.setQuotaData(size, inodes);

      return true;
   }

   return false;
}

/**
 * Gets the limits for a given ID range. The limits are returned in the given QuotaDataList. The
 * map is read locked during the execution.
 *
 * @param rangeStart       lowest ID in range
 * @param rangeEnd         highest ID in range (inclusive)
 * @param outQuotaDataList retrieved quota data is appended to this list.
 * @returns true if a limit was found for every ID in the range.
 */
bool QuotaStoreLimits::getQuotaLimitForRange(const unsigned rangeStart, const unsigned rangeEnd,
   QuotaDataList& outQuotaDataList)
{

   RWLockGuard const lock(limitsRWLock, SafeRWLock_READ);

   for (QuotaDataMapIter iter  = limits.lower_bound(rangeStart),
                          end  = limits.upper_bound(rangeEnd);
                         iter != end;
                       ++iter)
   {
         outQuotaDataList.push_back(iter->second);
   }

   return outQuotaDataList.size() == rangeEnd - rangeStart + 1;
}

/**
 * returns a map with all quota limits in the given map
 *
 * @param outMap contains all quota Limits
 */
QuotaDataMap QuotaStoreLimits::getAllQuotaLimits()
{
   RWLockGuard const lock(limitsRWLock, SafeRWLock_READ);

   return limits;
}

/**
 * Updates or inserts QuotaData into the map. The map is write locked during the update.
 *
 * @param quotaData the QuotaData to insert or to update
 * @return true if limits successful updated or inserted
 */
void QuotaStoreLimits::addOrUpdateLimit(const QuotaData quotaData)
{
#ifdef BEEGFS_DEBUG
   if (quotaData.getType() != this->quotaType)
      LogContext(this->logContext).log(Log_ERR, "Given QuotaDataType doesn't match to the "
         "QuotaDataType of the store.");
#endif

   RWLockGuard const lock(limitsRWLock, SafeRWLock_WRITE);

   addOrUpdateLimitUnlocked(quotaData);
}

/**
 * Updates or inserts QuotaData into the map. The map is NOT locked during the update.
 *
 * @param quotaData the QuotaData to insert or to update
 * @return true if limits successful updated or inserted
 */
void QuotaStoreLimits::addOrUpdateLimitUnlocked(const QuotaData quotaData)
{
#ifdef BEEGFS_DEBUG
   if (quotaData.getType() != this->quotaType)
      LogContext(this->logContext).log(Log_ERR, "Given QuotaDataType doesn't match to the "
         "QuotaDataType of the store.");
#endif

   // remove quota limit if size and inode value is zero
   if( (quotaData.getSize() == 0) && (quotaData.getInodes() == 0) )
      this->limits.erase(quotaData.getID() );
   else // update quota limit
      this->limits[quotaData.getID()] = quotaData;
}

/**
 * Updates or inserts a List of QuotaData into the map. The map is write locked during the update.
 *
 * @param quotaDataList the List of QuotaData to insert or to update
 * @return true if all limits successful updated or inserted, if one or more updates/inserts fails
 *         the return value is false
 */
void QuotaStoreLimits::addOrUpdateLimits(const QuotaDataList& quotaDataList)
{
   RWLockGuard const lock(limitsRWLock, SafeRWLock_WRITE);

   for (QuotaDataListConstIter iter = quotaDataList.begin(); iter != quotaDataList.end(); iter++)
   {
#ifdef BEEGFS_DEBUG
      if (iter->getType() != this->quotaType)
         LogContext(this->logContext).log(Log_ERR, "Given QuotaDataType doesn't match to the "
            "QuotaDataType of the store.");
#endif

      addOrUpdateLimitUnlocked(*iter);
   }
}

/**
 * Loads the quota limits form a file. The map is write locked during the load. Marks the store as
 * clean also when an error occurs.
 *
 * @return true if limits are successful loaded
 */
bool QuotaStoreLimits::loadFromFile()
{
   LogContext log(this->logContext);

   bool retVal = false;
   boost::scoped_array<char> buf;
   size_t readRes;

   struct stat statBuf;
   int retValStat;

   if(!this->storePath.length() )
      return false;

   RWLockGuard const lock(limitsRWLock, SafeRWLock_WRITE);

   int fd = open(this->storePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      log.log(Log_DEBUG, "Unable to open limits file: " + this->storePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_unlock;
   }

   retValStat = fstat(fd, &statBuf);
   if(retValStat)
   { // stat failed
      log.log(Log_WARNING, "Unable to stat limits file: " + this->storePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_stat;
   }

   buf.reset(new (std::nothrow) char[statBuf.st_size]);
   if (!buf)
   {
      LOG(GENERAL, WARNING, "Memory allocation failed");
      goto err_stat;
   }

   readRes = read(fd, buf.get(), statBuf.st_size);
   if(readRes <= 0)
   { // reading failed
      log.log(Log_WARNING, "Unable to read limits file: " + this->storePath + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   { // parse contents
      Deserializer des(buf.get(), readRes);
      des % this->limits;
      retVal = des.good();
   }

   if (!retVal)
      log.logErr("Could not deserialize limits from file: " + this->storePath);

err_stat:
   close(fd);

err_unlock:

   return retVal;
}

/**
 * Stores the quota limits into a file. The map is write locked during the save.
 *
 * @return true if the limits are successful stored
 */
bool QuotaStoreLimits::saveToFile()
{
   LogContext log(this->logContext);

   bool retVal = false;

   boost::scoped_array<char> buf;
   ssize_t bufLen;
   ssize_t writeRes;

   if(!this->storePath.length() )
      return false;

   RWLockGuard const lock(limitsRWLock, SafeRWLock_READ);

   Path quotaDataPath(this->storePath);
   if(!StorageTk::createPathOnDisk(quotaDataPath, true))
   {
      log.logErr("Unable to create directory for quota data: " + quotaDataPath.str() );
      return false;
   }

   // create/trunc file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(this->storePath.c_str(), openFlags, 0666);
   if(fd == -1)
   { // error
      log.logErr("Unable to create limits file: " + this->storePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_unlock;
   }

   // file created => store data
   bufLen = serializeIntoNewBuffer(limits, buf);
   if (bufLen < 0)
   {
      log.logErr("Unable to serialize limits file: " + this->storePath + ".");
      goto err_closefile;
   }

   writeRes = write(fd, buf.get(), bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      log.logErr("Unable to store limits file: " + this->storePath + ". " +
         "SysErr: " + System::getErrString() );

      goto err_closefile;
   }

   retVal = true;

   // error compensation
err_closefile:
   close(fd);

err_unlock:

   return retVal;
}

void QuotaStoreLimits::clearLimits()
{
   RWLockGuard safeRWLock(limitsRWLock, SafeRWLock_WRITE);
   limits.clear();

   int unlinkRes = unlink(storePath.c_str());

   if ( (unlinkRes != 0) && (errno != ENOENT) )
   {
      LOG(QUOTA, WARNING, "Could not delete file", ("path", storePath), sysErr);
   }
}

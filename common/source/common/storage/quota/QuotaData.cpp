#include <common/app/log/LogContext.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/StorageTk.h>
#include "QuotaData.h"

/**
 * Loads the quota data from a file. The map is write locked during the load. Marks the store as
 * clean also when an error occurs.
 *
 * @param map the map with the QuotaData to load
 * @param path the path to the file
 * @return true if quota data are successful loaded
 */
bool QuotaData::loadQuotaDataMapForTargetFromFile(QuotaDataMapForTarget& outMap,
   const std::string& path)
{
   LogContext log("LoadQuotaDataMap");

   bool retVal = false;
   boost::scoped_array<char> buf;
   size_t readRes;

   struct stat statBuf;
   int retValStat;

   if(!path.length() )
      return false;

   int fd = open(path.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      log.log(Log_DEBUG, "Unable to open quota data file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   retValStat = fstat(fd, &statBuf);
   if(retValStat)
   { // stat failed
      log.log(Log_WARNING, "Unable to stat quota data file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      goto err_stat;
   }

   buf.reset(new (std::nothrow) char[statBuf.st_size]);
   if (!buf)
   {
      LOG(QUOTA, WARNING, "Memory allocation failed");
      goto err_stat;
   }

   readRes = read(fd, buf.get(), statBuf.st_size);
   if(readRes <= 0)
   { // reading failed
      log.log(Log_WARNING, "Unable to read quota data file: " + path + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   { // parse contents
      Deserializer des(buf.get(), readRes);
      des % outMap;
      retVal = des.good();
   }

   if (!retVal)
      log.logErr("Could not deserialize quota data from file: " + path);
#ifdef BEEGFS_DEBUG
   else
   {
      int targetCounter = 0;
      int dataCounter = 0;

      for(QuotaDataMapForTargetIter targetIter = outMap.begin(); targetIter != outMap.end();
         targetIter++)
      {
         targetCounter++;

         for(QuotaDataMapIter iter = targetIter->second.begin(); iter != targetIter->second.end();
            iter++)
            dataCounter++;
      }

      log.log(Log_DEBUG, StringTk::intToStr(dataCounter) + " quota data of " +
         StringTk::intToStr(targetCounter) + " targets are loaded from file " + path);
   }
#endif

err_stat:
   close(fd);

   return retVal;
}

/**
 * Stores the quota data into a file. The map is write locked during the save.
 *
 * @param map the map with the QuotaData to store
 * @param path the path to the file
 * @return true if the limits are successful stored
 */
bool QuotaData::saveQuotaDataMapForTargetToFile(const QuotaDataMapForTarget &map,
   const std::string& path)
{
   LogContext log("SaveQuotaDataMap");

   bool retVal = false;

   boost::scoped_array<char> buf;
   ssize_t bufLen;
   ssize_t writeRes;

   if(!path.length() )
      return false;

   Path quotaDataPath(path);
   if(!StorageTk::createPathOnDisk(quotaDataPath, true) )
   {
      log.logErr("Unable to create directory for quota data: " + quotaDataPath.str() );
      return false;
   }

   // create/trunc file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(path.c_str(), openFlags, 0666);
   if(fd == -1)
   { // error
      log.logErr("Unable to create quota data file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   // file created => store data
   bufLen = serializeIntoNewBuffer(map, buf);
   if(bufLen < 0)
   {
      log.logErr("Unable to store quota data file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      goto err_closefile;
   }

   writeRes = write(fd, buf.get(), bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      log.logErr("Unable to store quota data file: " + path + ". " +
         "SysErr: " + System::getErrString() );

      goto err_closefile;
   }
#ifdef BEEGFS_DEBUG
   else
   {
      int targetCounter = 0;
      int dataCounter = 0;

      for(QuotaDataMapForTargetConstIter targetIter = map.begin(); targetIter != map.end();
         targetIter++)
      {
         targetCounter++;

         for(QuotaDataMapConstIter iter = targetIter->second.begin();
                                   iter != targetIter->second.end();
                                   iter++)
            dataCounter++;
      }

      log.log(Log_DEBUG, StringTk::intToStr(dataCounter) + " quota data of " +
         StringTk::intToStr(targetCounter) + " targets are stored to file " + path);
   }
#endif

   retVal = true;

   // error compensation
err_closefile:
   close(fd);

   return retVal;
}

void QuotaData::quotaDataMapToString(const QuotaDataMap& map, QuotaDataType quotaDataType,
   std::ostringstream* outStream)
{
   std::string quotaDataTypeStr;

   if(quotaDataType == QuotaDataType_USER)
      quotaDataTypeStr = "user";
   else
   if(quotaDataType == QuotaDataType_GROUP)
      quotaDataTypeStr = "group";


   *outStream << map.size() << " used quota for " << quotaDataTypeStr << " IDs: " << std::endl;

   for(QuotaDataMapConstIter idIter = map.begin(); idIter != map.end(); idIter++)
   {
      *outStream << "ID: " << idIter->second.getID()
                << " size: " << idIter->second.getSize()
                << " inodes: " << idIter->second.getInodes()
                << std::endl;
   }
}

void QuotaData::quotaDataMapForTargetToString(const QuotaDataMapForTarget& map,
   QuotaDataType quotaDataType, std::ostringstream* outStream)
{
   std::string quotaDataTypeStr;

   if(quotaDataType == QuotaDataType_USER)
      quotaDataTypeStr = "user";
   else
   if(quotaDataType == QuotaDataType_GROUP)
      quotaDataTypeStr = "group";

   *outStream << "Quota data of " << map.size() << " targets." << std::endl;
   for(QuotaDataMapForTargetConstIter iter = map.begin(); iter != map.end(); iter++)
   {
      uint16_t targetNumID = iter->first;
      *outStream << "Used quota for " << quotaDataTypeStr << " IDs on target: " << targetNumID
         << std::endl;

      QuotaData::quotaDataMapToString(iter->second, quotaDataType, outStream);
   }
}

void QuotaData::quotaDataListToString(const QuotaDataList& list, std::ostringstream* outStream)
{
   for(QuotaDataListConstIter idIter = list.begin(); idIter != list.end(); idIter++)
      *outStream << "ID: " << idIter->getID()
                << " size: " << idIter->getSize()
                << " inodes: " << idIter->getInodes()
                << std::endl;
}

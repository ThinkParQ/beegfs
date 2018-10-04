#include <common/storage/quota/Quota.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "QuotaTk.h"

#include <boost/lexical_cast.hpp>

#include <dlfcn.h>
// xfs/xfs.h includes linux/fs.h, which (sometimes) defines MS_RDONLY.
// sys/mount.h is the canonical source of MS_RDONLY, but declares it as an enum - which is then
// broken by linux/fs.h when included after it
#include <sys/mount.h>
#include <xfs/xfs.h>
#include <xfs/xqm.h>

#define QUOTATK_ZFS_USER_QUOTA          "userused@"
#define QUOTATK_ZFS_GROUP_QUOTA         "groupused@"
#define QUOTATK_ZFS_USER_INODE_QUOTA    "userobjused@"
#define QUOTATK_ZFS_GROUP_INODE_QUOTA   "groupobjused@"

/**
 * Get quota data for a single ID and append it to the outQuotaDataList.
 *
 * @param id the user or group ID to check
 * @param type the type of the ID user or group, enum QuotaDataType
 * @param blockDevices the QuotaBlockDevice to check
 * @param outQuotaDataList the return list with the quota data
 * @param session a session for all required lib handles if zfs is used, it can be an uninitialized
 *        session, the initialization can be done by this function
 *
 * @return false on error (in which case outData is empty)
 */
bool QuotaTk::appendQuotaForID(unsigned id, QuotaDataType type, QuotaBlockDeviceMap* blockDevices,
   QuotaDataList* outQuotaDataList, ZfsSession* session)
{
   QuotaData data(id, type);
   bool retVal = checkQuota(blockDevices, &data, session);

   if(retVal && (data.getSize() != 0 || data.getInodes() != 0) )
      outQuotaDataList->push_back(data);

   return retVal;
}

/**
 * get quota for the a range of IDs
 *
 * @param blockDevices the QuotaBlockDevice to check
 * @param rangeStart the first ID of the ID range
 * @param rangeEnd the last ID of the ID range
 * @param type the quota data ID type user/group, QuotaDataType_...
 * @param outQuotaDataList the return list with the quota data
 * @param session a session for all required lib handles if zfs is used, it can be an uninitialized
 *        session, the initialization can be done by this function
 *
 * @return false on error (in which case outData is empty)
 */
bool QuotaTk::requestQuotaForRange(QuotaBlockDeviceMap* blockDevices, unsigned rangeStart,
   unsigned rangeEnd, QuotaDataType type, QuotaDataList* outQuotaDataList, ZfsSession* session)
{
   bool retVal = true;

   for(unsigned id = rangeStart; id <= rangeEnd; id++)
   {
      bool errorVal = appendQuotaForID(id, type, blockDevices, outQuotaDataList, session);

      if(!errorVal)
         retVal = false;
   }

   return retVal;
}

/**
 * get quota for the a range of IDs
 *
 * @param blockDevices the QuotaBlockDevice to check
 * @param idList the list with the IDs to check
 * @param type the quota data ID type user/group, QuotaDataType_...
 * @param outQuotaDataList the return list with the quota data
 * @param session a session for all required lib handles if zfs is used, it can be an uninitialized
 *        session, the initialization can be done by this function
 *
 * @return false on error (in which case outData is empty)
 */
bool QuotaTk::requestQuotaForList(QuotaBlockDeviceMap* blockDevices, UIntList* idList,
   QuotaDataType type, QuotaDataList* outQuotaDataList, ZfsSession* session)
{
   bool retVal = true;

   for(UIntListIter iter = idList->begin(); iter != idList->end(); iter++)
   {
      bool errorVal = appendQuotaForID(*iter, type, blockDevices, outQuotaDataList, session);

      if(!errorVal)
         retVal = false;
   }

   return retVal;
}

/**
 * get QuotaData for the given QuotaData which is initialized with type and ID
 *
 * @param blockDevices the QuotaBlockDevice to check
 * @param outData needs to be initialized with type and ID
 * @param session a session for all required lib handles if zfs is used, it can be an uninitialized
 *        session, the initialization can be done by this function
 *
 * @return false on error (in which case outData is not initialized)
 */
bool QuotaTk::checkQuota(QuotaBlockDeviceMap* blockDevices, QuotaData* outData, ZfsSession* session)
{
   bool retVal = true;
   int errorCode = 0;

   dqblk quotaData;                                                  // required for extX
   fs_disk_quota xfsQuotaData;                                       // required for XFS
   QuotaData tmpQuotaData(outData->getID(), outData->getType() );    // required for ZFS

   QuotaDataType quotaType = outData->getType();

   for(QuotaBlockDeviceMapIter iter = blockDevices->begin(); iter != blockDevices->end(); iter++)
   {
      QuotaBlockDeviceFsType fstype = iter->second.getFsType();
      if(fstype == QuotaBlockDeviceFsType_XFS)
      {
         if(quotaType == QuotaDataType_USER)
            errorCode = quotactl(QCMD(Q_XGETQUOTA, USRQUOTA),
               iter->second.getBlockDevicePath().c_str(),
               outData->getID(), (caddr_t)&xfsQuotaData);
         else
         if (quotaType == QuotaDataType_GROUP)
            errorCode = quotactl(QCMD(Q_XGETQUOTA, GRPQUOTA),
               iter->second.getBlockDevicePath().c_str(),
               outData->getID(), (caddr_t)&xfsQuotaData);
         else
         {
            LOG(QUOTA, ERR, "Quota request - useless quota type.",
                  ("Type", QuotaData::QuotaDataTypeToString(outData->getType())),
                  ("ID", outData->getID()));

            return false;
         }
      }
      else
      if ( (fstype == QuotaBlockDeviceFsType_ZFS) || (fstype == QuotaBlockDeviceFsType_ZFSOLD) )
      {
         if(!session->isSessionValid() )
         {
            if(!session->initZfsSession(Program::getApp()->getDlOpenHandleLibZfs() ) )
               return false;
         }

         if(QuotaTk::requestQuotaFromZFS(&iter->second, iter->first, &tmpQuotaData, session))
            errorCode = FhgfsOpsErr_SUCCESS;
         else
         {
            LOG(QUOTA, ERR, "Quota request - useless quota type.",
                  ("Type", QuotaData::QuotaDataTypeToString(outData->getType())),
                  ("ID", outData->getID()));

            return false;
         }
      }
      else
      {
         if(quotaType == QuotaDataType_USER)
            errorCode = quotactl(QCMD(Q_GETQUOTA, USRQUOTA),
               iter->second.getBlockDevicePath().c_str(),
               outData->getID(), (caddr_t)&quotaData);
         else
         if (quotaType == QuotaDataType_GROUP)
            errorCode = quotactl(QCMD(Q_GETQUOTA, GRPQUOTA),
               iter->second.getBlockDevicePath().c_str(),
               outData->getID(), (caddr_t)&quotaData);
         else
         {
            LOG(QUOTA, ERR, "Quota request - useless quota type.",
                  ("Type", QuotaData::QuotaDataTypeToString(outData->getType())),
                  ("ID", outData->getID()));

            return false;
         }
      }

      if (errorCode != 0)
      {
         errorCode = errno;

         // ignore error if no quota for user found (ESRCH) , especially for XFS (ENOENT)
         if( (errorCode != ESRCH) &&
            !((iter->second.getFsType() == QuotaBlockDeviceFsType_XFS) && (errorCode == ENOENT) ) )
         {
            LOG(QUOTA, ERR, "Quota request - quotactl failed.",
               ("Type", QuotaData::QuotaDataTypeToString(outData->getType())),
               ("ID", outData->getID()),
               ("sysErr", System::getErrString(errorCode)),
               ("fstype", boost::lexical_cast<std::string>(iter->second.getFsType())));

            return false;
         }

         continue;
      }

      if(fstype == QuotaBlockDeviceFsType_XFS)
      {
         uint64_t blocks = UnitTk::quotaBlockCountToByte(xfsQuotaData.d_bcount,
            iter->second.getFsType() );
         outData->forceMergeQuotaDataCounter(blocks, xfsQuotaData.d_icount);
      }
      else
      if ( (fstype == QuotaBlockDeviceFsType_ZFS) || (fstype == QuotaBlockDeviceFsType_ZFSOLD) )
      {
         if(tmpQuotaData.isValid() )
         {
            uint64_t blocks = UnitTk::quotaBlockCountToByte(tmpQuotaData.getSize(),
               iter->second.getFsType() );
            outData->forceMergeQuotaDataCounter(blocks, tmpQuotaData.getInodes() );
            tmpQuotaData.setIsValid(false);
         }
         else
         {
            // set return value to false, but do not abort the quota request from the other targets,
            // maybe the other targets returns valid values
            LOG(QUOTA, ERR, "Quota request - values not valid.",
                  ("Type", QuotaData::QuotaDataTypeToString(outData->getType())),
                  ("ID", outData->getID()));

            retVal = false;
         }
      }
      else
      {
         if((quotaData.dqb_valid & QIF_USAGE) != 0)
         {
            uint64_t blocks = UnitTk::quotaBlockCountToByte(quotaData.dqb_curspace,
               iter->second.getFsType());

            outData->forceMergeQuotaDataCounter(blocks, quotaData.dqb_curinodes);
         }
         else
         {
            // set return value to false, but do not abort the quota request from the other targets,
            // maybe the other targets returns valid values
            LOG(QUOTA, ERR, "Quota request - values not valid.",
                  ("Type", QuotaData::QuotaDataTypeToString(outData->getType())),
                  ("ID", outData->getID()));

            retVal = false;
         }
      }
   }

   return retVal;
}

/**
 * initialize the lib zfs
 *
 * @param dlHandle the handle from dlopen to the libzfs
 * @return the libzfs handle (libzfs_handle_t*), which is required for all libzfs calls
 */
void* QuotaTk::initLibZfs(void* dlHandle)
{
   App* app = Program::getApp();

   char* dlErrorString;
   void* (*libzfs_init)();

   libzfs_init = (void* (*)())dlsym(dlHandle, "libzfs_init");
   if ( (dlErrorString = dlerror() ) != NULL)
   {
      LOG(QUOTA, ERR, "Error during dynamic load of function libzfs_init.", dlErrorString);
      app->setLibZfsErrorReported(true);
      return NULL;
   }

   void* libZfsHandle = (*libzfs_init)();
   if(!libZfsHandle)
   {
      LOG(QUOTA, ERR, "Error during create of libzfs_handle_t.");
      app->setLibZfsErrorReported(true);
      return NULL;
   }

   return libZfsHandle;
}

/**
 * uninitialize the lib zfs
 *
 * @param dlHandle the handle from dlopen to the libzfs
 * @param libZfsHandle the libzfs handle (libzfs_handle_t*) to the libzfs handle (libzfs_handle_t*)
 * @return false in case of error
 */
bool QuotaTk::uninitLibZfs(void* dlHandle, void* libZfsHandle)
{
   App* app = Program::getApp();

   if( (!dlHandle) || (!libZfsHandle) )
      return true;

   void (*libzfs_fini)(void*);
   char* dlErrorString;

   libzfs_fini = (void (*)(void*))dlsym(dlHandle, "libzfs_fini");
   if ( (dlErrorString = dlerror() ) != NULL)
   {
      LOG(QUOTA, ERR, "Error during dynamic load of function libzfs_fini.", dlErrorString);
      app->setLibZfsErrorReported(true);
      return false;
   }

   (*libzfs_fini)(libZfsHandle);

   libZfsHandle = NULL;

   return true;
}

/**
 * checks if all required functions of the installed libzfs are available
 *
 * @param blockDevice a QuotaBlockDevice for a test the get quota workflow
 * @param targetNumID the targetNumID of the storage target
 * @return true if all function pointers are available, false if a error occurred
 */
bool QuotaTk::checkRequiredLibZfsFunctions(QuotaBlockDevice* blockDevice, uint16_t targetNumID)
{
   App* app = Program::getApp();

   ZfsSession session;

   // check if all function names in the lib available
   if(!session.initZfsSession(app->getDlOpenHandleLibZfs() ) )
   {
      if(!app->getConfig()->getQuotaDisableZfsSupport() )
         LOG(QUOTA, ERR, "Cannot initialize libzfs. "
            "The quota system for all zfs blockdevices/pools on this machine won't work.");

      return false;
   }

   // check if the signature of all function pointers are compatible
   QuotaData quotaData(0, QuotaDataType_USER); // test request for user root
   if (!requestQuotaFromZFS(blockDevice, targetNumID, &quotaData, &session))
   {  // quota update failed; try to get quota without inode quota (only supported on zfs >= 0.7.4)
      // i.e. set the fsType to QuotaBlockDeviceFsType_ZFSOLD and try again
      // if it still doesn't work, libzfs is not compatible at all, if it works now we fall back to
      // quota without inode quota
      blockDevice->setFsType(QuotaBlockDeviceFsType_ZFSOLD);
      if (!requestQuotaFromZFS(blockDevice, targetNumID, &quotaData, &session))
         return false;
   }

   // check function pointers which are required in case of errors happens
   std::string errorDec( (*session.libzfs_error_description)(session.getlibZfsHandle() ) );
   std::string errorAct( (*session.libzfs_error_action)(session.getlibZfsHandle() ) );

   return true;
}

/**
 * get QuotaData from zfs for the given QuotaData which is initialized with type and ID
 *
 * @param blockDevice the QuotaBlockDevice to check
 * @param targetNumID the targetNumID of the storage target
 * @param outData needs to be initialized with type and ID
 * @param session a session for all required lib handles if zfs is used, it can be an uninitialized
 *        session, the initialization can be done by this function
 * @return false on error (in which case outData is not initialized)
 */
bool QuotaTk::requestQuotaFromZFS(QuotaBlockDevice* blockDevice, uint16_t targetNumID,
   QuotaData* outData, ZfsSession* session)
{
   if(!session->isSessionValid() )
   {
      outData->setIsValid(false);
      return false;
   }

   void* zfsHandle = session->getZfsDeviceHandle(targetNumID, blockDevice->getBlockDevicePath() );
   if(!zfsHandle)
      return false;

   uint64_t usedSizeValue = 0;
   uint64_t usedInodesValue = 0;

   std::string sizeProp;
   std::string inodeProp;

   if(outData->getType() == QuotaDataType_USER)
   {
      sizeProp = QUOTATK_ZFS_USER_QUOTA + StringTk::uintToStr(outData->getID());
      inodeProp = QUOTATK_ZFS_USER_INODE_QUOTA + StringTk::uintToStr(outData->getID());
   }
   else
   {
      sizeProp = QUOTATK_ZFS_GROUP_QUOTA + StringTk::uintToStr(outData->getID());
      inodeProp = QUOTATK_ZFS_GROUP_INODE_QUOTA + StringTk::uintToStr(outData->getID());
   }

   if ((*session->zfs_prop_get_userquota_int)(zfsHandle, sizeProp.c_str(), &usedSizeValue))
   {
      LOG(QUOTA, ERR, "Error during request quota data.",
            ("ErrorAction", (*session->libzfs_error_action)(session->getlibZfsHandle())),
            ("ErrorDescription", (*session->libzfs_error_description)(session->getlibZfsHandle()))
         );
      return false;
   }

    if (blockDevice->getFsType() != QuotaBlockDeviceFsType_ZFSOLD) // no inode support on zfs<=0.7.4
    {
       if ((*session->zfs_prop_get_userquota_int)(zfsHandle, inodeProp.c_str(), &usedInodesValue))
       {
           // could not read inode quota (which is only supported since zfs 0.7.4)
           // log the error and set 0 as inode quota
          LOG(QUOTA, ERR, "Inode quota could not be requested. Please note that inode quota on"
                " ZFS is not supported for zfs versions prior to 0.7.4.",
                ("ZFS Error", (*session->libzfs_error_description)(session->getlibZfsHandle())));
           return false;
       }
    }

   outData->setQuotaData(usedSizeValue, usedInodesValue);

   return true;
}

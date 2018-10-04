#include <toolkit/QuotaTk.h>
#include <program/Program.h>
#include "ZfsSession.h"

#include <dlfcn.h>



/**
 * dummy constructor, constructs a invalid ZfsSession, this constructor doesn't require the libzfs,
 * the method initZfsSession can make the session valid if the installed libzfs is working with this
 * implementation. The session must be initialized if a zfs block device is detected.
 */
ZfsSession::ZfsSession()
{
   this->dlOpenHandleLibZfs = NULL;
   this->libZfsHandle = NULL;
   this->zfs_open = NULL;
   this->zfs_prop_get_userquota_int = NULL;
   this->libzfs_error_description = NULL;
   this->libzfs_error_action = NULL;

   this->isValid = false;
}

/**
 * destructor
 */
ZfsSession::~ZfsSession()
{
   QuotaTk::uninitLibZfs(this->dlOpenHandleLibZfs, this->libZfsHandle);
}

/**
 * initialize the ZfsSession, sets the valid flag if all zfs handels and function pointers are
 * successful created
 *
 * @param dlOpenHandleLib the handle from dlopen to the libzfs
 * @return true if all zfs handels and function pointers are successful created
 */
bool ZfsSession::initZfsSession(void* dlOpenHandleLib)
{
   App* app = Program::getApp();

   if(this->isValid)
      return true;

   if(!dlOpenHandleLib)
      return false;


   char* dlErrorString;

   this->dlOpenHandleLibZfs = dlOpenHandleLib;
   this->isValid = true;


   this->libZfsHandle = QuotaTk::initLibZfs(this->dlOpenHandleLibZfs);
   if(!this->libZfsHandle)
   {
      this->isValid = false;
      return false;
   }


   this->zfs_open = (void* (*)(void*, const char*, int))dlsym(this->dlOpenHandleLibZfs, "zfs_open");
   if ( (dlErrorString = dlerror() ) != NULL)
   {
      LOG(QUOTA, ERR, "Error during dynamic load of function zfs_open.", dlErrorString);
      app->setLibZfsErrorReported(true);
      this->isValid = false;
      return false;
   }


   this->zfs_prop_get_userquota_int = (int (*)(void*, const char*, uint64_t*))dlsym(
      this->dlOpenHandleLibZfs, "zfs_prop_get_userquota_int");
   if ( (dlErrorString = dlerror() ) != NULL)
   {
      LOG(QUOTA, ERR, "Error during dynamic load of function zfs_prop_get_userquota_int.",
            dlErrorString);
      app->setLibZfsErrorReported(true);
      this->isValid = false;
      return false;
   }


   this->libzfs_error_description = (char* (*)(void*))dlsym(this->dlOpenHandleLibZfs,
      "libzfs_error_description");
   if ( (dlErrorString = dlerror() ) != NULL)
   {
      LOG(QUOTA, ERR, "Error during dynamic load of function libzfs_error_description.",
            dlErrorString);
      app->setLibZfsErrorReported(true);
      this->isValid = false;
      return false;
   }


   this->libzfs_error_action = (char* (*)(void*))dlsym(this->dlOpenHandleLibZfs,
      "libzfs_error_action");
   if ( (dlErrorString = dlerror() ) != NULL)
   {
      LOG(QUOTA, ERR, "Error during dynamic load of function libzfs_error_action.",
         dlErrorString);
      app->setLibZfsErrorReported(true);
      this->isValid = false;
      return false;
   }

   return this->isValid;
}

/**
 * checks for a existing blockdevice/pool handle (zfs_handle_t*) or creates a new zfs
 * blockdevice/pool handle (zfs_handle_t*) for the given path an targetNumID
 *
 * @param targetNumID the targetNumID of the storage target
 * @param path the path of the blockdevice/poolname
 * @return returns a zfs blockdevice/pool handle (zfs_handle_t*) or NULL if a error occurs
 */
void* ZfsSession::getZfsDeviceHandle(uint16_t targetNumID, std::string path)
{
   if(!this->isValid)
      return NULL;

   ZfsPoolHandleMapIter iter = fsHandles.find(targetNumID);

   if(iter != this->fsHandles.end() )
      return iter->second;
   else
   {
      void* newFsHandle = (*this->zfs_open)(this->libZfsHandle, path.c_str(), ZFSSESSION_ZFS_TYPE);
      if(!newFsHandle)
      {
         LOG(QUOTA, ERR, "Error during create of zfs pool handle.",
               ("ErrorAction", (*this->libzfs_error_action)(this->libZfsHandle)),
               ("ErrorDescription", (*this->libzfs_error_description)(this->libZfsHandle)));
      }
      else
      {
         fsHandles.insert(ZfsPoolHandleMapMapVal(targetNumID, newFsHandle) );
         return newFsHandle;
      }
   }

   return NULL;
}

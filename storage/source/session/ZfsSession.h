#ifndef SESSION_ZFSSESSION_H_
#define SESSION_ZFSSESSION_H_






#define ZFSSESSION_ZFS_TYPE            1 // must be same as ZFS_TYPE_FILESYSTEM from libzfs

typedef std::map<uint16_t, void*> ZfsPoolHandleMap; // targetNumID => zfs_handle_t*
typedef ZfsPoolHandleMap::iterator ZfsPoolHandleMapIter;
typedef ZfsPoolHandleMap::value_type ZfsPoolHandleMapMapVal;


class ZfsSession
{
   public:
      ZfsSession();
      virtual ~ZfsSession();
      bool initZfsSession(void* dlOpenHandleLib);

      void* getZfsDeviceHandle(uint16_t targetNumID, std::string path);

      int (*zfs_prop_get_userquota_int)(void*, const char*, uint64_t*); // fp to get quota data
      char* (*libzfs_error_description)(void*); // fp to get error description
      char* (*libzfs_error_action)(void*); // fp to get action during the error occurs


   private:
      void* dlOpenHandleLibZfs;     // handle of dlOpen from the libzfs
      void* libZfsHandle;           // handle of the libzfs from libzfs_init()

      ZfsPoolHandleMap fsHandles;   // handle of a ZFS pool, the type is zfs_handle_t*

      void* (*zfs_open)(void*, const char*, int); // fp to open zfs pool

      bool isValid;


   public:
      /**
       * getter and setter
       */
      void* getlibZfsHandle()
      {
         return this->libZfsHandle;
      }

      bool isSessionValid()
      {
         return this->isValid;
      }
};

#endif /* SESSION_ZFSSESSION_H_ */

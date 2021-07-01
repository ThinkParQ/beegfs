#ifndef FILEEVENT_H_
#define FILEEVENT_H_

#include <common/toolkit/serialization/Serialization.h>

enum class FileEventType
{
   FLUSH = 0,
   TRUNCATE = 1,
   SETATTR = 2,
   CLOSE_WRITE = 3,
   CREATE = 4,
   MKDIR = 5,
   MKNOD = 6,
   SYMLINK = 7,
   RMDIR = 8,
   UNLINK = 9,
   HARDLINK = 10,
   RENAME = 11,
   READ = 12,
};

template<>
struct SerializeAs<FileEventType>
{
   typedef uint32_t type;
};

struct FileEvent
{
   FileEventType type;
   std::string path;
   bool targetValid;
   std::string target;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->type
         % obj->path;

      ctx % obj->targetValid;
      if (obj->targetValid)
         ctx % obj->target;
   }
};

#endif

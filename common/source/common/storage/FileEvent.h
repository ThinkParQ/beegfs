#pragma once

#include <common/toolkit/serialization/Serialization.h>

enum class FileEventType
{
   FLUSH = 1,
   TRUNCATE = 2,
   SETATTR = 3,
   CLOSE_WRITE = 4,
   CREATE = 5,
   MKDIR = 6,
   MKNOD = 7,
   SYMLINK = 8,
   RMDIR = 9,
   UNLINK = 10,
   HARDLINK = 11,
   RENAME = 12,
   OPEN_READ = 13,
   OPEN_WRITE = 14,
   OPEN_READ_WRITE = 15,
   LAST_WRITER_CLOSED = 16
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


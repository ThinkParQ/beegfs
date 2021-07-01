#ifndef FILEEVENT_H_
#define FILEEVENT_H_

#include <common/toolkit/Serialization.h>

struct dentry;

enum FileEventType
{
   FileEventType_FLUSH,
   FileEventType_TRUNCATE,
   FileEventType_SETATTR,
   FileEventType_CLOSE_WRITE,
   FileEventType_CREATE,
   FileEventType_MKDIR,
   FileEventType_MKNOD,
   FileEventType_SYMLINK,
   FileEventType_RMDIR,
   FileEventType_UNLINK,
   FileEventType_HARDLINK,
   FileEventType_RENAME,
   FileEventType_READ,
};

struct FileEvent
{
   uint32_t eventType; /* enum FileEventType */
   const char* path; /* NULL if invalid/could not be determined (empty is also allowed) */
   const char* target; /* link target for link, new name for rename */

   unsigned long pathPagePFN;
   unsigned long targetPagePFN;
};

void FileEvent_init(struct FileEvent* event, enum FileEventType eventType, struct dentry* dentry);
void FileEvent_uninit(struct FileEvent* event);

static inline void FileEvent_setTargetStr(struct FileEvent* event, const char* target)
{
   if (event->targetPagePFN)
      kfree((void *)event->targetPagePFN);
   else
      kfree(event->target);

   event->targetPagePFN = 0;
   event->target = kstrdup(target, GFP_NOFS);
}

void FileEvent_setTargetDentry(struct FileEvent* event, struct dentry* dentry);

void FileEvent_serialize(SerializeCtx* ctx, const struct FileEvent* event);

/* the empty file event object is a valid event that may be destroyed, but that holds no
 * state itself. thus, it may also be reinitialized without being destroyed first. */
#define FILEEVENT_EMPTY {0, NULL, NULL, 0, 0}

#endif

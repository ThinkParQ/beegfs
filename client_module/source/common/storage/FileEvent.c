#include "FileEvent.h"

#include <linux/fs.h>

void FileEvent_init(struct FileEvent* event, enum FileEventType eventType, struct dentry* dentry)
{
   memset(event, 0, sizeof(*event));

   event->eventType = eventType;

   if (!dentry)
      return;

   event->pathPagePFN = (unsigned long) kmalloc(4096, GFP_NOFS);
   if (!event->pathPagePFN)
      return;

   event->path = dentry_path_raw(dentry, (char*) event->pathPagePFN, PAGE_SIZE);
   if (IS_ERR(event->path))
      event->path = NULL;
}

void FileEvent_uninit(struct FileEvent* event)
{
   if (event->pathPagePFN)
      kfree((void *)event->pathPagePFN);

   FileEvent_setTargetStr(event, NULL);
}

void FileEvent_setTargetDentry(struct FileEvent* event, struct dentry* dentry)
{
   FileEvent_setTargetStr(event, NULL);

   if (!dentry)
      return;

   event->targetPagePFN = (unsigned long) kmalloc(4096, GFP_NOFS);
   if (!event->targetPagePFN)
      return;

   event->target = dentry_path_raw(dentry, (char*) event->targetPagePFN, PAGE_SIZE);
   if (IS_ERR(event->target))
      event->target = NULL;
}

void FileEvent_serialize(SerializeCtx* ctx, const struct FileEvent* event)
{
   Serialization_serializeUInt(ctx, event->eventType);

   if (event->path)
      Serialization_serializeStr(ctx, strlen(event->path), event->path);
   else
      Serialization_serializeStr(ctx, 0, "");

   Serialization_serializeBool(ctx, event->target != NULL);
   if (event->target != NULL)
      Serialization_serializeStr(ctx, strlen(event->target), event->target);
}

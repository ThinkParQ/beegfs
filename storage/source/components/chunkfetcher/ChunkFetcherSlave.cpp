#include "ChunkFetcherSlave.h"

#include <program/Program.h>

#include <boost/static_assert.hpp>
#include <libgen.h>

ChunkFetcherSlave::ChunkFetcherSlave(uint16_t targetID):
   PThread("ChunkFetcherSlave-" + StringTk::uintToStr(targetID) ),
   log("ChunkFetcherSlave-" + StringTk::uintToStr(targetID) ),
   isRunning(false),
   targetID(targetID)
{
}

ChunkFetcherSlave::~ChunkFetcherSlave()
{
}

void ChunkFetcherSlave::run()
{
   setIsRunning(true);

   try
   {
      registerSignalHandler();

      walkAllChunks();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   setIsRunning(false);
}

/*
 * walk over all chunks in that target
 */
void ChunkFetcherSlave::walkAllChunks()
{
   App* app = Program::getApp();

   log.log(Log_DEBUG, "Starting chunks walk...");

   const auto& target = *app->getStorageTargets()->getTargets().at(targetID);

   const auto& targetPath = target.getPath().str();

   // walk over "normal" chunks (i.e. no mirrors)
   std::string walkPath = targetPath + "/" + CONFIG_CHUNK_SUBDIR_NAME;
   if(!walkChunkPath(walkPath, 0, walkPath.size() ) )
      return;

   // let's find out if this target is part of a buddy mirror group and if it is the primary
   // target; if it is, walk over buddy mirror directory
   bool isPrimaryTarget;
   uint16_t buddyGroupID = app->getMirrorBuddyGroupMapper()->getBuddyGroupID(this->targetID,
      &isPrimaryTarget);

   if (isPrimaryTarget)
   {
      walkPath = targetPath + "/" CONFIG_BUDDYMIRROR_SUBDIR_NAME;
      if(!walkChunkPath(walkPath, buddyGroupID, walkPath.size() ) )
         return;
   }

   log.log(Log_DEBUG, "End of chunks walk.");
}

bool ChunkFetcherSlave::walkChunkPath(const std::string& path, uint16_t buddyGroupID,
   unsigned basePathLen)
{
   DIR* dir = ::opendir(path.c_str() );
   if(!dir)
   {
      LOG(GENERAL, WARNING, "Could not open directory.", path, targetID, sysErr);
      Program::getApp()->getChunkFetcher()->setBad();
      return false;
   }

   int readRes;
   bool result = true;

   std::string pathBuf = path;
   pathBuf.push_back('/');

   while(!getSelfTerminate())
   {
      ::dirent* item;

      // we really want struct struct dirent to contain a reasonably sized array for the filename
      BOOST_STATIC_ASSERT(sizeof(item->d_name) >= NAME_MAX + 1);

#if USE_READDIR_R
      ::dirent entry;
      readRes = ::readdir_r(dir, &entry, &item);
#else
      errno = 0;
      item = readdir(dir);
      readRes = item ? 0 : errno;
#endif
      if(readRes != 0)
      {
         LOG(GENERAL, WARNING, "readdir failed.", path, targetID, sysErr(readRes));
         result = false;
         break;
      }

      if(!item)
         break;

      if(::strcmp(item->d_name, ".") == 0 || ::strcmp(item->d_name, "..") == 0)
         continue;

      pathBuf.resize(path.size() + 1);
      pathBuf += item->d_name;

      struct stat statBuf;

      int statRes = ::stat(pathBuf.c_str(), &statBuf);
      if(statRes)
      {
         LOG(GENERAL, WARNING, "Could not stat directory.", ("path", pathBuf), targetID, sysErr);
         result = false;
         break;
      }

      if(S_ISDIR(statBuf.st_mode) )
      {
         result = walkChunkPath(pathBuf, buddyGroupID, basePathLen);
         if(!result)
            break;
      }
      else
      {
         const char* relativeChunkPath = pathBuf.c_str() + basePathLen + 1;

         // get only the dirname part of the path
         char* tmpPathCopy = strdup(relativeChunkPath);
         Path savedPath(dirname(tmpPathCopy) );

         free(tmpPathCopy);

         FsckChunk fsckChunk(item->d_name, targetID, savedPath, statBuf.st_size, statBuf.st_blocks,
            statBuf.st_ctime, statBuf.st_mtime, statBuf.st_atime, statBuf.st_uid, statBuf.st_gid,
            buddyGroupID);

         Program::getApp()->getChunkFetcher()->addChunk(fsckChunk);
      }
   }

   ::closedir(dir);

   if (getSelfTerminate())
      result = false;

   if(!result)
      Program::getApp()->getChunkFetcher()->setBad();

   return result;
}

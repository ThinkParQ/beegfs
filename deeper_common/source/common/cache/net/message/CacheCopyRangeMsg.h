#ifndef NET_MESSAGE_CACHECOPYRANGEMSG_H_
#define NET_MESSAGE_CACHECOPYRANGEMSG_H_


#include <common/cache/components/worker/CacheWork.h>
#include <common/net/message/NetMessage.h>



class CacheCopyRangeMsg : public NetMessageSerdes<CacheCopyRangeMsg>
{
   public:
      /*
       * superclass, not for direct use
       */
      CacheCopyRangeMsg(unsigned short msgType, std::string sourcePath, std::string destPath,
         int cacheFlags, off_t startPos, size_t numBytes) : BaseType(msgType), path(sourcePath),
         destPath(destPath), cacheFlags(cacheFlags), startPos(startPos), numBytes(numBytes) {};

      /*
       * superclass, not for direct use, deserialization only
       */
      CacheCopyRangeMsg(unsigned short msgType) : BaseType(msgType) {};

      virtual ~CacheCopyRangeMsg() {};


   protected:
      std::string path;
      std::string destPath;
      int cacheFlags;
      off_t startPos;
      size_t numBytes;


   public:
      std::string getSourcePath()
      {
         return path;
      }

      std::string getDestPath()
      {
         return destPath;
      }

      int getCacheFlags()
      {
         return cacheFlags;
      }

      off_t getStartPos()
      {
         return this->startPos;
      }

      size_t getNumBytes()
      {
         return this->numBytes;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->cacheFlags
            % obj->path
            % obj->destPath
            % obj->startPos
            % obj->numBytes;
      }

      /**
       * Generates a key for the hash map.
       *
       * @param outKey The update key for the hash map.
       */
      void getKey(CacheWorkKey& outKey)
      {
         outKey.sourcePath = path;
         outKey.destPath = destPath;
         outKey.type = CacheWorkType_NONE;
      }
};

#endif /* NET_MESSAGE_CACHECOPYRANGEMSG_H_ */

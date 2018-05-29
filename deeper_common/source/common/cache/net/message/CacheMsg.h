#ifndef NET_MESSAGE_CACHEMSG_H_
#define NET_MESSAGE_CACHEMSG_H_


#include <common/cache/components/worker/CacheWork.h>
#include <common/net/message/NetMessage.h>



class CacheMsg : public NetMessageSerdes<CacheMsg>
{
   public:
      /*
       * superclass, not for direct use
       */
      CacheMsg(unsigned short msgType, std::string sourcePath, std::string destPath,
         int cacheFlags) : BaseType(msgType), path(sourcePath), destPath(destPath),
         cacheFlags(cacheFlags) {};

      /*
       * superclass, not for direct use, deserialization only
       */
      CacheMsg(unsigned short msgType) : BaseType(msgType) {};

      virtual ~CacheMsg() {};


   protected:
      std::string path;
      std::string destPath;
      int cacheFlags;


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

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->cacheFlags
            % obj->path
            % obj->destPath;
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

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "sourcePath: " + path +
            "; destPath: " + destPath +
            "; deeperFlags: " + StringTk::intToStr(cacheFlags);
      }
};

#endif /* NET_MESSAGE_CACHECOPYMSG_H_ */

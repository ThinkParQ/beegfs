#ifndef MKLOCALFILEMSG_H_
#define MKLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/striping/StripePattern.h>

class MkLocalFileMsg : public NetMessageSerdes<MkLocalFileMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param pattern just a reference, so do not free it as long as you use this object!
       */
      MkLocalFileMsg(Path* path, StripePattern* pattern) : BaseType(NETMSGTYPE_MkLocalFile)
      {
         this->path = path;
         this->pattern = pattern;
      }

      MkLocalFileMsg() : BaseType(NETMSGTYPE_MkLocalFile)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->path, obj->parsed.path)
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern);
      }

   private:
      // for serialization
      Path* path; // not owned by this object!
      StripePattern* pattern; // not owned by this object!

      // for deserialization
      struct {
         Path path;
         std::unique_ptr<StripePattern> pattern;
      } parsed;


   public:
      Path& getPath()
      {
         return *path;
      }

      StripePattern& getPattern()
      {
         return *pattern;
      }
};

#endif /*MKLOCALFILEMSG_H_*/

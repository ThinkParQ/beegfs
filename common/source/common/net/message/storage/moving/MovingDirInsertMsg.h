#ifndef MOVINGDIRINSERTMSG_H_
#define MOVINGDIRINSERTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>

class MovingDirInsertMsg : public MirroredMessageBase<MovingDirInsertMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param serialBuf serialized dir-link;
       * just a reference, so do not free it as long as you use this object!
       */
      MovingDirInsertMsg(EntryInfo* toDirInfo, const std::string& newName,
         const char* serialBuf, unsigned serialBufLen)
          : BaseType(NETMSGTYPE_MovingDirInsert),
            serialBuf(serialBuf),
            serialBufLen(serialBufLen),
            newName(newName),
            toDirInfoPtr(toDirInfo)
      { }

      MovingDirInsertMsg() : BaseType(NETMSGTYPE_MovingDirInsert)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->toDirInfoPtr, obj->toDirInfo)
            % serdes::stringAlign4(obj->newName)
            % obj->serialBufLen
            % serdes::rawBlock(obj->serialBuf, obj->serialBufLen);

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->dirTimestamps;
      }

      bool supportsMirroring() const { return true; }

   private:
      const char* serialBuf;
      uint32_t serialBufLen;

      std::string newName;

      // for serialization
      EntryInfo* toDirInfoPtr;

      // for deserialization
      EntryInfo toDirInfo;

   protected:
      MirroredTimestamps dirTimestamps;

   public:

      EntryInfo* getToDirInfo(void)
      {
         return &this->toDirInfo;
      }

      const std::string& getNewName(void)
      {
         return this->newName;
      }

      const char* getSerialBuf(void)
      {
         return this->serialBuf;
      }

      uint32_t getSerialBufLen() const { return serialBufLen; }
};

#endif /*MOVINGDIRINSERTMSG_H_*/

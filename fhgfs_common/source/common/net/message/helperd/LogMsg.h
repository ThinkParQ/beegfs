#ifndef LOGMSG_H_
#define LOGMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class LogMsg : public NetMessageSerdes<LogMsg>
{
   public:

      /**
       * @param context just a reference, so do not free it as long as you use this object!
       * @param logMsg just a reference, so do not free it as long as you use this object!
       */
      LogMsg(int level, int threadID, const char* threadName, const char* context,
         const char* logMsg) :
         BaseType(NETMSGTYPE_Log)
      {
         this->level = level;
         this->threadID = threadID;

         this->threadName = threadName;
         this->threadNameLen = strlen(threadName);

         this->context = context;
         this->contextLen = strlen(context);

         this->logMsg = logMsg;
         this->logMsgLen = strlen(logMsg);
      }

      LogMsg() : BaseType(NETMSGTYPE_Log)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->level
            % obj->threadID
            % serdes::rawString(obj->threadName, obj->threadNameLen)
            % serdes::rawString(obj->context, obj->contextLen)
            % serdes::rawString(obj->logMsg, obj->logMsgLen);
      }



   private:
      int32_t level;
      int32_t threadID;
      unsigned threadNameLen;
      const char* threadName;
      unsigned contextLen;
      const char* context;
      unsigned logMsgLen;
      const char* logMsg;


   public:

      // getters & setters
      int getLevel()
      {
         return level;
      }

      int getThreadID()
      {
         return threadID;
      }

      const char* getThreadName()
      {
         return threadName;
      }

      const char* getContext()
      {
         return context;
      }

      const char* getLogMsg()
      {
         return logMsg;
      }

};

#endif /*LOGMSG_H_*/

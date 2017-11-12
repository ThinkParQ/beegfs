#ifndef LOGMSG_H_
#define LOGMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class LogMsg : public NetMessage
{
   public:
      
      /**
       * @param context just a reference, so do not free it as long as you use this object!
       * @param logMsg just a reference, so do not free it as long as you use this object!
       */
      LogMsg(int level, int threadID, const char* threadName, const char* context,
         const char* logMsg) :
         NetMessage(NETMSGTYPE_Log)
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


   protected:
      LogMsg() : NetMessage(NETMSGTYPE_Log)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // level
            Serialization::serialLenInt() + // threadID
            Serialization::serialLenStr(threadNameLen) +
            Serialization::serialLenStr(contextLen) +
            Serialization::serialLenStr(logMsgLen);
      }


   private:
      int level;
      int threadID;
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

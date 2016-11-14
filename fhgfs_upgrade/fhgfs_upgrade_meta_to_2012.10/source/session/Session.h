#ifndef SESSION_H_
#define SESSION_H_

#include <common/Common.h>
#include "SessionFileStore.h"


class Session
{
   public:
      Session(std::string sessionID) : sessionID(sessionID) {}
   
   private:
      std::string sessionID;
      
      SessionFileStore files;
      
   public:
      // getters & setters
      std::string getSessionID()
      {
         return sessionID;
      }
      
      SessionFileStore* getFiles()
      {
         return &files;
      }
      
};


#endif /*SESSION_H_*/

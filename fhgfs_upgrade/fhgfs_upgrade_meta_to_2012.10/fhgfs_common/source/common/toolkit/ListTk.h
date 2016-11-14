#ifndef LISTTK_H_
#define LISTTK_H_

#include <common/Common.h>

class ListTk
{
   public:
      
      
   private:
      ListTk() {}

      
   public:
      // inliners
      
      /**
       * @param outPos zero-based list position if found, undefined otherwise
       */
      static bool listContains(std::string searchStr, StringList* list, ssize_t* outPos)
      {
         (*outPos) = 0;
         
         for(StringListIter iter=list->begin(); iter != list->end(); iter++)
         {
            if(!searchStr.compare(*iter) )
               return true;
            
            (*outPos)++;
         }
         
         (*outPos) = -1;
         
         return false;
      }
};


#endif /*LISTTK_H_*/

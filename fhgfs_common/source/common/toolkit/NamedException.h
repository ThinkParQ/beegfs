#ifndef NAMEDEXCEPTION_H_
#define NAMEDEXCEPTION_H_

#include <common/Common.h>

#define DECLARE_NAMEDSUBEXCEPTION(exceptionClass, exceptionName, superException) \
   class exceptionClass : public superException \
   { \
      public: \
         exceptionClass (const char* message) : \
            superException( exceptionName, message) \
         {} \
          \
         exceptionClass (const std::string message) : \
            superException( exceptionName, message) \
         {} \
          \
         virtual ~exceptionClass() throw() \
         {} \
          \
      protected: \
         exceptionClass (const char* subExceptionName, const char* message) : \
            superException ( subExceptionName, message) \
         {} \
          \
         exceptionClass (const char* subExceptionName, const std::string message) : \
            superException ( subExceptionName, message) \
         {} \
   };

#define DECLARE_NAMEDEXCEPTION(exceptionClass, exceptionName) \
   DECLARE_NAMEDSUBEXCEPTION(exceptionClass, exceptionName, NamedException)

class NamedException : public std::exception
{
   protected:
      NamedException(const char* exceptionName, const char* message) :
         name(exceptionName), msg(message)
      {
         whatMsg = msg;
      }

      NamedException(const char* exceptionName, const std::string message) :
         name(exceptionName), msg(message)
      {
         whatMsg = msg;
      }

   public:
      virtual ~NamedException() throw()
      {
      }

      virtual const char* what() const throw()
      {
         return whatMsg.c_str();
      }

   private:
      std::string name;
      std::string msg;

      std::string whatMsg; /* required because the final string must not be locally
            defined in the what()-method. (it would be removed from the stack as soon as the method
            ends in that case) */

};

#endif /*NAMEDEXCEPTION_H_*/

#ifndef DATABASEEXCEPTION_H_
#define DATABASEEXCEPTION_H_

#include <exception>

enum ErrorCode
{
   DB_CONNECT_ERROR=0,
   DB_QUERY_ERROR=1
};

class DatabaseException :  public std::exception
{
   private:
      const char* errorDesc; // a short description of the error
      short code; // a error code, see the enum ErrorCode
      const char* query; //the query that caused the error (optional)

   public:
      DatabaseException(ErrorCode code, const char* errorDesc) :
         std::exception()
      {
         this->code = code;
         this->errorDesc = errorDesc;
         this->query = "";
      }

      DatabaseException(ErrorCode code, const char* errorDesc,
         const char* query) : std::exception()
      {
         this->code = code;
         this->errorDesc = errorDesc;
         this->query = query;
      }

      virtual const char* what() const throw ()
      {
         return errorDesc;
      }

      const char* queryStr() const throw ()
      {
         return query;
      }

      short errorCode() const throw ()
      {
         return code;
      }
};

#endif /*DATABASEEXCEPTION_H_*/

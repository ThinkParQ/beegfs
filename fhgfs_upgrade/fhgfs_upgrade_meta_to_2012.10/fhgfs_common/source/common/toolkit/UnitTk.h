#ifndef UNITTK_H_
#define UNITTK_H_

#include <common/Common.h>


class UnitTk
{
   public:
      static int64_t gigabyteToByte(double gigabyte);
      static int64_t megabyteToByte(double megabyte);
      static int64_t kilobyteToByte(double kilobyte);
      static double byteToXbyte(int64_t bytes, std::string *unit);
      static double byteToXbyte(int64_t bytes, std::string *unit, bool round);
      static int64_t xbyteToByte(double xbyte, std::string unit);

      static int64_t strHumanToInt64(const char* s);
      static std::string int64ToHumanStr(int64_t a);
      

   private:
      UnitTk() {};
      
   
   public:
      // inliners
      static int64_t strHumanToInt64(std::string s)
      {
         return strHumanToInt64(s.c_str() );
      }
};


#endif /* UNITTK_H_ */

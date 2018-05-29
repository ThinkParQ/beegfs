#ifndef AUTH_H_
#define AUTH_H_

#include <common/Common.h>
#include <toolkit/security/SecurityTk.h>


class Auth
{
   public:
      static void init();
      static bool getInformationDisabled();
      static void setInformationDisabled(bool value);
      static void setPassword(std::string username, std::string password);
      static std::string getPassword(std::string username);


   private:
      static std::string informationPW;
      static std::string adminPW;

   public:
      // inliners

      static bool hasAdmin(std::string username, std::string pw)
      {
         if ((username == "admin") && (pw != "") && (SecurityTk::DoMD5(pw) == adminPW))
         {
            return true;
         }
         else
         {
            return false;
         }
      }

      static bool hasInformation(std::string username, std::string pw)
      {
         if (getInformationDisabled())
         {
            return true;
         }
         else if ((username == "information") && (pw != "") &&
            (SecurityTk::DoMD5(pw) == informationPW))
         {
            return true;
         }
         else if (hasAdmin(username, pw))
         {
            return true;
         }
         else
         {
            return false;
         }
      }
};

#endif /*AUTH_H_*/

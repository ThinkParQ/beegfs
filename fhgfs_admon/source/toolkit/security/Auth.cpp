#include "Auth.h"
#include <program/Program.h>

std::string Auth::informationPW = "";
std::string Auth::adminPW = "";

void Auth::init()
{
   informationPW = Program::getApp()->getDB()->getPassword("information");
   adminPW = Program::getApp()->getDB()->getPassword("admin");
}

void Auth::setPassword(std::string username, std::string password)
{
   Program::getApp()->getDB()->setPassword(username, SecurityTk::DoMD5(password));
   init();
}

std::string Auth::getPassword(std::string username)
{
   if (username.compare("information") == 0)
   {
      return informationPW;
   }
   else if (username.compare("admin") == 0)
   {
      return adminPW;
   }
   else
      return "";
}

bool Auth::getInformationDisabled()
{
   return Program::getApp()->getDB()->getDisabled("information");
}

void Auth::setInformationDisabled(bool value)
{
   Program::getApp()->getDB()->setDisabled("information", value);
}

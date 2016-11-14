#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/app/log/LogContext.h>
#include <common/toolkit/StringTk.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/storage/creating/MkDirMsg.h>
#include <common/storage/Path.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <net/message/NetMessageFactory.h>
#include "StartupTests.h"


bool StartupTests::perform()
{
   LogContext log("StartupTests");
   
   log.log(1, "Running startup tests...");

   bool testRes =
      testLogging() &&
      testPath() &&
      testNICsHwAddr() &&
      testNICs();
   
   log.log(1, "Startup tests complete");
   
   return testRes;
}

bool StartupTests::testLogging()
{
   LogContext log("testLogging");
   
   log.log(1, "Test level '1'");
   log.log(2, "Test level '2'");
   log.log(3, "Test level '3'");
   log.logErr("Test level 'err'");
   
   return true;
}


bool StartupTests::testPath()
{
   LogContext log("testPath");

   StringVector origPathElems;
   origPathElems.push_back("xyz");
   origPathElems.push_back("tralala");
   origPathElems.push_back("abc.pdf");
   
   std::string slash = "/";
   
   std::string pathStr;
   for(StringVectorIter iter = origPathElems.begin(); iter != origPathElems.end(); iter++)
      pathStr += slash + *iter;
   
   log.log(3, std::string("Path: '") + pathStr + std::string("'") ); 
   
   Path path(pathStr);
   
   if(path.size() != origPathElems.size() )
   {
      log.logErr(std::string("Element count is wrong: ") +
         StringTk::intToStr(path.size()) );
      return false;
   }
   
   for (size_t i = 0; i < path.size(); i++)
   {
      if(path[i] != origPathElems[i])
      {
         log.logErr("Strings do not match: " +
            std::string("'") + path[i] + std::string("'") +
            std::string(" / ") +
            std::string("'") + origPathElems[i] + std::string("'") );
            
         return false;
      }
      
      log.log(3, std::string("pathElem: '") + path[i] + std::string("'") ); 
   }
   
   log.log(2, "OK");
   
   return true;
}


bool StartupTests::testNICsHwAddr()
{
   LogContext log("testNICsHwAddr");

   log.log(2, "Checking array size...");
   
   if(IFHWADDRLEN != 6)
   {
      log.logErr("Problem: Size != 6");
      
      return false;
   }

   log.log(2, "OK.");
   
   return true;
}

bool StartupTests::testNICs()
{
   LogContext log("testNICs");
   
   log.log(2, "Querying all NICs...");
   
   NicAddressList list;
   StringList allowedInterfaces;
   
   if(!NetworkInterfaceCard::findAll(&allowedInterfaces, true, true, &list) )
   {
      log.logErr("Failed to query NICs");
      return false;
   }
   
   if(!list.size() )
   {
      log.log(1, "Couldn't find any NIC. Cancelling test.");
      return false;
   }
   
   log.log(1, std::string("Discovered ") +
      StringTk::intToStr(list.size() ) + std::string(" NICs") );

   log.log(2, std::string("Here comes the list of usable NICs (ordered by preference)...") );
   
   list.sort(&NetworkInterfaceCard::nicAddrPreferenceComp);
      
   for(NicAddressListIter iter = list.begin(); iter != list.end(); iter++)
   {
      log.log(2, NetworkInterfaceCard::nicAddrToString(&(*iter) ) );
   }
   
   log.log(2, "Querying the first interface of the list by its name...");
   
   NicAddress nicAddr;
   if(!NetworkInterfaceCard::findByName(list.begin()->name, &nicAddr) )
   {
      log.logErr("Query by name failed. Cancelling test.");
      return false;
   }
   
   log.log(2, NetworkInterfaceCard::nicAddrToString(&nicAddr) );
      
   return true;
}




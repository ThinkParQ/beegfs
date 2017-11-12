#include "Mode.h"


/**
 * Check given config for a remaining arg and print it as invalid.
 *
 * @return true if a remaining invalid argument was found in the given config.
 */
bool Mode::checkInvalidArgs(const StringMap* cfg)
{
   if(cfg->empty() )
      return false;

   if(cfg->size() == 1)
      std::cerr << "Invalid argument: " << cfg->begin()->first << std::endl;
   else
   { // multiple invalid args
      std::cerr << "Found " << cfg->size() << " invalid arguments. One of them is: " <<
         cfg->begin()->first << std::endl;
   }

   return true;
}

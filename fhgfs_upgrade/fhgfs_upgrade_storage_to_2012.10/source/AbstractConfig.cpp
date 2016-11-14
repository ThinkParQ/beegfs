#include "StringTk.h"
#include "AbstractConfig.h"


AbstractConfig::AbstractConfig(int argc, char** argv)
{
   // initConfig(..) must be called by derived classes
   // because of the virtual init method loadDefaults()
   
   this->argc = argc;
   this->argv = argv;
}

/**
 * Note: This must not be called from the AbstractConfig (or base class) constructor, because it
 * calls virtual methods of the sub classes.
 *
 * @param enableException: true to throw an exception when an unknown config element is found,
 * false to ignore it.
 * @param addDashes true to prepend dashes to defaults and config file keys (used e.g. by fhgfs-ctl)
 */
void AbstractConfig::initConfig(int argc, char** argv, bool enableException, bool addDashes)
{
   // load and apply args to see whether we have a cfgFile
   loadDefaults(addDashes);
   loadFromArgs(argc, argv);
   
   applyConfigMap(enableException, addDashes);
   
   initImplicitVals();
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes true to prepend "--" to all config keys.
 */
void AbstractConfig::loadDefaults(bool addDashes)
{
}

/**
 * @param enableException: true to throw an exception when an unknown config element is found,
 * false to ignore it.
 * @param addDashes true to prepend "--" to tested config keys for matching.
 */
bool AbstractConfig::applyConfigMap(bool enableException, bool addDashes)
{
   for(StringMapIter iter = configMap.begin(); iter != configMap.end(); )
   {
      bool unknownElement = false;
      
      { // unknown element occurred
         unknownElement = true;
      }
      
      if(unknownElement)
      { // just skip the unknown element
         iter++;
      }
      else
      { // remove this element from the map
         iter = eraseFromConfigMap(iter);
      }
   }

   return true;
}

void AbstractConfig::initImplicitVals()
{
   // nothing to be done here (just a dummy so that derived classes don't have to impl this)
}

/**
 * Removes an element from the config map and returns an iterator that is positioned right
 * after the removed element
 */
StringMapIter AbstractConfig::eraseFromConfigMap(StringMapIter iter)
{
   StringMapIter nextIter(iter);

   nextIter++; // save position after the erase element
   
   configMap.erase(iter);
   
   return nextIter;
}

/**
 * Note: No addDashes param here, because the user should specify dashes himself on the command line
 * if they are needed.
 */
void AbstractConfig::loadFromArgs(int argc, char** argv)
{
   for(int i=1; i < argc; i++)
      MapTk::addLineToStringMap(argv[i], &configMap);
}


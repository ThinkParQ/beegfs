#include <common/toolkit/StringTk.h>
#include <common/toolkit/StorageTk.h>
#include "AbstractConfig.h"


AbstractConfig::AbstractConfig(int argc, char** argv) throw(InvalidConfigException)
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
   throw(InvalidConfigException)
{
   // load and apply args to see whether we have a cfgFile
   loadDefaults(addDashes);
   loadFromArgs(argc, argv);
   
   applyConfigMap(enableException, addDashes);
   
   if(this->cfgFile.length() )
   { // there is a config file specified
      // start over again and include the config file this time
      this->configMap.clear();
      
      std::string tmpCfgFile = this->cfgFile; /* need tmp variable here to make sure we don't
         override the value when we use call loadDefaults() again below, because subclasses have
         direct access to this->cfgFile */

      loadDefaults(addDashes);
      loadFromFile(tmpCfgFile.c_str(), addDashes);
      loadFromArgs(argc, argv);

      applyConfigMap(enableException, addDashes);
   }

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
void AbstractConfig::applyConfigMap(bool enableException, bool addDashes)
   throw(InvalidConfigException)
{
}

void AbstractConfig::initImplicitVals() throw(InvalidConfigException)
{
   // nothing to be done here (just a dummy so that derived classes don't have to impl this)
}

/**
 * Initialize interfaces list from interfaces file if the list is currently empty and the filename
 * is not empty.
 *
 * @param inoutConnInterfacesList will be initialized from file (comma-separated) if it was empty
 *
 * @throw InvalidConfigException if interfaces filename and interface list are both not empty
 */
void AbstractConfig::initInterfacesList(std::string connInterfacesFile,
   std::string& inoutConnInterfacesList) throw(InvalidConfigException)
{
   // make sure not both (list and file) were specified
   if(!inoutConnInterfacesList.empty() && !connInterfacesFile.empty() )
      throw InvalidConfigException(
         "connInterfacesFile and connInterfacesList cannot be used together");

   if(!inoutConnInterfacesList.empty() )
      return; // interfaces already given as list => nothing to do

   if(connInterfacesFile.empty() )
      return; // no interfaces file given => nothing to do


   // load interfaces from file...

   StringList loadedInterfacesList;

   loadStringListFile(connInterfacesFile.c_str(), loadedInterfacesList);

   inoutConnInterfacesList = StringTk::implode(',', loadedInterfacesList, true);
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
 * @param addDashes true to prepend "--" to every config key.
 */
void AbstractConfig::loadFromFile(const char* filename, bool addDashes)
   throw(InvalidConfigException)
{
   if(!addDashes)
   { // no dashes needed => load directly into configMap
      MapTk::loadStringMapFromFile(filename, &configMap);
      return;
   }

   /* we need to add dashes to keys => use temporary map with real keys and then copy to actual
      config map with prepended dashes. */

   StringMap tmpMap;

   MapTk::loadStringMapFromFile(filename, &tmpMap);

   for(StringMapIter iter = tmpMap.begin(); iter != tmpMap.end(); iter++)
      configMapRedefine(iter->first, iter->second, addDashes);
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


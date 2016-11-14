/*
 * Some methods for struct/class CfgClientStatsOptions
 */

#include <app/App.h>
#include <modes/modehelpers/ModeHelper.h>
#include <common/clientstats/CfgClientStatsOptions.h>

/**
 * Read parameters given by the user.
 *
 * @return APPCODE_...
 */
int CfgClientStatsOptions::getParams(StringMap* cfg)
{
   StringMapIter iter;

   iter = cfg->find(MODECLIENTSTATS_ARG_INTERVALSECS);
   if(iter != cfg->end() )
   {
      this->intervalSecs = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_MAXLINES);
   if(iter != cfg->end() )
   {
      this->maxLines = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_ALLSTATS);
   if(iter != cfg->end() )
   {
      this->allStats = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_PERINTERVAL);
   if(iter != cfg->end() )
   {
      this->perInterval = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_RWUNIT);
   if(iter != cfg->end() )
   {
      std::string unit = iter->second;
      if (unit != "B" && unit != "MiB")
      {
         std::cerr << "Unsupported unit!" << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
      this->unit = unit;

      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_SHOWNAMES);
   if(iter != cfg->end() )
   {
      this->showNames = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODECLIENTSTATS_ARG_FILTER);
   if(iter != cfg->end() )
   {
      this->filter = iter->second;
      cfg->erase(iter);
   }

   iter = cfg->find(MODE_ARG_NODETYPE);
   if (iter == cfg->end() )
      cfg->insert(StringMapVal(MODE_ARG_NODETYPE, MODE_ARG_NODETYPE_META) ); // default nodetype

   this->nodeType = ModeHelper::nodeTypeFromCfg(cfg);

   if ( (this->nodeType != NODETYPE_Meta) && (this->nodeType != NODETYPE_Storage) )
   {
      std::cerr << "Invalid nodetype." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   // nodeID given by the user
   iter = cfg->begin();
   if(iter != cfg->end() )
   {
      bool isNumericRes = StringTk::isNumeric(iter->first);
      if(!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << iter->first << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->nodeID = iter->first;
      cfg->erase(iter);
   }

   if(ModeHelper::checkInvalidArgs(cfg) )
         return APPCODE_INVALID_CONFIG;

   return APPCODE_NO_ERROR;
}

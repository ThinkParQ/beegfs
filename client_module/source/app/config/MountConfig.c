#include <app/config/MountConfig.h>
#include <common/toolkit/list/StrCpyList.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/Common.h>

#include <linux/parser.h>


enum {
   /* Mount options that take string arguments */
   Opt_cfgFile,
   Opt_logStdFile,
   Opt_sysMgmtdHost,
   Opt_tunePreferredMetaFile,
   Opt_tunePreferredStorageFile,

   Opt_connInterfacesList,
   Opt_connAuthFile,
   Opt_connDisableAuthentication,

   /* Mount options that take integer arguments */
   Opt_logLevel,
   Opt_connPortShift,
   Opt_connMgmtdPort,
   Opt_sysMountSanityCheckMS,

   /* Mount options that take no arguments */
   Opt_grpid,

   Opt_err
};


static match_table_t fhgfs_mount_option_tokens =
{
   /* Mount options that take string arguments */
   { Opt_cfgFile, "cfgFile=%s" },
   { Opt_logStdFile, "logStdFile=%s" },
   { Opt_sysMgmtdHost, "sysMgmtdHost=%s" },
   { Opt_tunePreferredMetaFile, "tunePreferredMetaFile=%s" },
   { Opt_tunePreferredStorageFile, "tunePreferredStorageFile=%s" },

   { Opt_connInterfacesList, "connInterfacesList=%s" },
   { Opt_connAuthFile, "connAuthFile=%s" },
   { Opt_connDisableAuthentication, "connDisableAuthentication=%s" },

   /* Mount options that take integer arguments */
   { Opt_logLevel, "logLevel=%d" },
   { Opt_connPortShift, "connPortShift=%d" },
   { Opt_connMgmtdPort, "connMgmtdPort=%u" },
   { Opt_sysMountSanityCheckMS, "sysMountSanityCheckMS=%u" },

   { Opt_grpid, "grpid" },

   { Opt_err, NULL }
};



bool MountConfig_parseFromRawOptions(MountConfig* this, char* mountOptions)
{
   char* currentOption;

   if(!mountOptions)
   {
      printk_fhgfs_debug(KERN_INFO, "Mount options = <none>\n");
      return true;
   }


   printk_fhgfs_debug(KERN_INFO, "Mount options = '%s'\n", mountOptions);

   while( (currentOption = strsep(&mountOptions, ",") ) != NULL)
   {
      substring_t args[MAX_OPT_ARGS];
      int tokenID;

      if(!*currentOption)
         continue; // skip empty string

      tokenID = match_token(currentOption, fhgfs_mount_option_tokens, args);

      switch(tokenID)
      {
         /* Mount options that take STRING arguments */

         case Opt_cfgFile:
         {
            SAFE_KFREE(this->cfgFile);

            this->cfgFile = match_strdup(args);// (string kalloc'ed => needs kfree later)
         } break;

         case Opt_logStdFile:
         {
            SAFE_KFREE(this->logStdFile);

            this->logStdFile = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_sysMgmtdHost:
         {
            SAFE_KFREE(this->sysMgmtdHost);

            this->sysMgmtdHost = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_tunePreferredMetaFile:
         {
            SAFE_KFREE(this->tunePreferredMetaFile);

            this->tunePreferredMetaFile = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_tunePreferredStorageFile:
         {
            SAFE_KFREE(this->tunePreferredStorageFile);

            this->tunePreferredStorageFile = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_connInterfacesList:
         {
            SAFE_KFREE(this->connInterfacesList);

            this->connInterfacesList = match_strdup(args);
         } break;

         case Opt_connAuthFile:
         {
            SAFE_KFREE(this->connAuthFile);

            this->connAuthFile = match_strdup(args);
         } break;

         case Opt_connDisableAuthentication:
         {
            SAFE_KFREE(this->connDisableAuthentication);

            this->connDisableAuthentication = match_strdup(args);
         } break;

         /* Mount options that take INTEGER arguments */

         case Opt_logLevel:
         {
            if(match_int(args, &this->logLevel) )
               goto err_exit_invalid_option;

            this->logLevelDefined = true;
         } break;

         case Opt_connPortShift:
         {
            if(match_int(args, &this->connPortShift) )
               goto err_exit_invalid_option;

            this->connPortShiftDefined = true;
         } break;

         case Opt_connMgmtdPort:
         {
            if(match_int(args, &this->connMgmtdPort) )
               goto err_exit_invalid_option;

            this->connMgmtdPortDefined = true;
         } break;

         case Opt_sysMountSanityCheckMS:
         {
            if(match_int(args, &this->sysMountSanityCheckMS) )
               goto err_exit_invalid_option;

            this->sysMountSanityCheckMSDefined = true;
         } break;

         case Opt_grpid:
            this->grpid = true;
            break;

         default:
            goto err_exit_unknown_option;
      }
   }

   return true;


err_exit_unknown_option:
   printk_fhgfs(KERN_WARNING, "Unknown mount option: '%s'\n", currentOption);
   return false;

err_exit_invalid_option:
   printk_fhgfs(KERN_WARNING, "Invalid mount option: '%s'\n", currentOption);
   return false;
}

void MountConfig_showOptions(MountConfig* this, struct seq_file* sf)
{
   if (this->cfgFile)
      seq_printf(sf, ",cfgFile=%s", this->cfgFile);

   if (this->logStdFile)
      seq_printf(sf, ",logStdFile=%s", this->logStdFile);

   if (this->sysMgmtdHost)
      seq_printf(sf, ",sysMgmtdHost=%s", this->sysMgmtdHost);

   if (this->tunePreferredMetaFile)
      seq_printf(sf, ",tunePreferredMetaFile=%s", this->tunePreferredMetaFile);

   if (this->tunePreferredStorageFile)
      seq_printf(sf, ",tunePreferredStorageFile=%s", this->tunePreferredStorageFile);

   if (this->connInterfacesList)
      seq_printf(sf, ",connInterfacesList=%s", this->connInterfacesList);

   if (this->connAuthFile)
      seq_printf(sf, ",connAuthFile=%s", this->connInterfacesList);

   if (this->connDisableAuthentication)
      seq_printf(sf, ",connDisableAuthentication=%s", this->connInterfacesList);

   if (this->logLevelDefined)
      seq_printf(sf, ",logLevel=%d", this->logLevel);

   if (this->connPortShiftDefined)
      seq_printf(sf, ",connPortShift=%d", this->connPortShift);

   if (this->connMgmtdPortDefined)
      seq_printf(sf, ",connMgmtdPort=%u", this->connMgmtdPort);

   if (this->sysMountSanityCheckMSDefined)
      seq_printf(sf, ",sysMountSanityCheckMS=%u", this->sysMountSanityCheckMS);

   if (this->grpid)
      seq_printf(sf, ",grpid");
}

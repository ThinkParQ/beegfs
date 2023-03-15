#ifndef OPEN_MOUNTCONFIG_H_
#define OPEN_MOUNTCONFIG_H_

#include <common/Common.h>

#include <linux/seq_file.h>

struct MountConfig;
typedef struct MountConfig MountConfig;


static inline void MountConfig_init(MountConfig* this);
static inline MountConfig* MountConfig_construct(void);
static inline void MountConfig_uninit(MountConfig* this);
static inline void MountConfig_destruct(MountConfig* this);

extern bool MountConfig_parseFromRawOptions(MountConfig* this, char* mountOptions);
extern void MountConfig_showOptions(MountConfig* this, struct seq_file* sf);


struct MountConfig
{
   char* cfgFile;
   char* logStdFile;
   char* sysMgmtdHost;
   char* tunePreferredMetaFile;
   char* tunePreferredStorageFile;

   bool logLevelDefined; // true if the value has been specified
   bool connPortShiftDefined; // true if the value has been specified
   bool connMgmtdPortUDPDefined; // true if the value has been specified
   bool connMgmtdPortTCPDefined; // true if the value has been specified
   bool sysMountSanityCheckMSDefined; // true if the value has been specified

   int logLevel;
   unsigned connPortShift;
   unsigned connMgmtdPortUDP;
   unsigned connMgmtdPortTCP;
   unsigned sysMountSanityCheckMS;
   char* connInterfacesList;
};


void MountConfig_init(MountConfig* this)
{
   memset(this, 0, sizeof(*this) );
}

struct MountConfig* MountConfig_construct(void)
{
   struct MountConfig* this = (MountConfig*)os_kmalloc(sizeof(*this) );

   MountConfig_init(this);

   return this;
}

void MountConfig_uninit(MountConfig* this)
{
   SAFE_KFREE(this->cfgFile);
   SAFE_KFREE(this->logStdFile);
   SAFE_KFREE(this->sysMgmtdHost);
   SAFE_KFREE(this->tunePreferredMetaFile);
   SAFE_KFREE(this->tunePreferredStorageFile);
}

void MountConfig_destruct(MountConfig* this)
{
   MountConfig_uninit(this);

   kfree(this);
}


#endif /*OPEN_MOUNTCONFIG_H_*/

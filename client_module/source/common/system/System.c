#include <common/toolkit/StringTk.h>
#include <linux/utsname.h>
#include "System.h"

char* System_getHostnameCopy(void)
{
   // note: this is racy since 2.6.27 unexported uts_sem. however, NFS (fs/nfs/nfsroot.c) and
   // CIFS (fs/cifs/connect.c) use utsname()->nodename without the semaphore.
   // we'll have to keep an eye on it...

   char* hostnameOrig;
   char* hostnameCopy;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
   down_read(&uts_sem);
#endif

#ifdef KERNEL_HAS_SYSTEM_UTSNAME
   hostnameOrig = system_utsname.nodename;
#else
   hostnameOrig = utsname()->nodename;
#endif

   hostnameCopy = kmalloc(strlen(hostnameOrig)+1, GFP_KERNEL);
   if(likely(hostnameCopy) )
      strcpy(hostnameCopy, hostnameOrig);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
   up_read(&uts_sem);
#endif

   return hostnameCopy;
}

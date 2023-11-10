#include <common/net/sock/RDMASocket.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/NicAddressList.h>

#if 0
#include <linux/if_arp.h>
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <net/sock.h>
#endif

bool NicAddressList_equals(NicAddressList* this, NicAddressList* other)
{
   bool result = false;

   if (NicAddressList_length(this) == NicAddressList_length(other))
   {
      PointerListIter thisIter;
      PointerListIter otherIter;

      PointerListIter_init(&thisIter, (PointerList*) this);
      PointerListIter_init(&otherIter, (PointerList*) other);

      for (result = true;
           result == true && !PointerListIter_end(&thisIter) && !PointerListIter_end(&otherIter);
           PointerListIter_next(&thisIter), PointerListIter_next(&otherIter))
      {
         result = NicAddress_equals((NicAddress*) PointerListIter_value(&thisIter),
            (NicAddress*) PointerListIter_value(&otherIter));
      }
   }

   return result;
}

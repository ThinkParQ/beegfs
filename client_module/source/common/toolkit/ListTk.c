#include <common/nodes/ConnectionListIter.h>
#include <common/net/sock/NicAddressListIter.h>
#include <common/net/sock/NicAddressStatsListIter.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>
#include "ListTk.h"

#include <linux/sort.h>

struct nicaddr_sort_entry {
   StrCpyList* preferences;
   NicAddress* addr;
};

static int nicaddr_sort_comp(const void* l, const void* r)
{
   const struct nicaddr_sort_entry* lhs = l;
   const struct nicaddr_sort_entry* rhs = r;

   StrCpyListIter it;

   StrCpyListIter_init(&it, lhs->preferences);
   while (!StrCpyListIter_end(&it)) {
      const char* value = StrCpyListIter_value(&it);

      if (strcmp(value, lhs->addr->name) == 0)
         return -1;
      if (strcmp(value, rhs->addr->name) == 0)
         return 1;

      StrCpyListIter_next(&it);
   }

   if (NicAddress_preferenceComp(lhs->addr, rhs->addr))
      return -1;
   if (NicAddress_preferenceComp(rhs->addr, lhs->addr))
      return 1;

   return 0;
}



void ListTk_cloneNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone, bool includeTcp)
{
   NicAddressListIter iter;

   NicAddressList_init(nicListClone);

   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);
      if (includeTcp || nicAddr->nicType != NICADDRTYPE_STANDARD)
      {
         NicAddress* nicAddrClone;

         // clone element

         nicAddrClone = (NicAddress*)os_kmalloc(sizeof(NicAddress) );

         memcpy(nicAddrClone, nicAddr, sizeof(NicAddress) );

         // append to the clone list
         NicAddressList_append(nicListClone, nicAddrClone);
      }
   }
}


/**
 * Returns a sorted clone of a nicList.
 *
 * Note: the comparison implemented here is a valid only if preferences contains
 * all possible names ever encountered, or none at all.
 */
void ListTk_cloneSortNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone,
      StrCpyList* preferences)
{
   NicAddressListIter listIter;

   struct nicaddr_sort_entry* list, *p;

   /* i'm so sorry. we can't indicate failure here without a lot of work in App, and the
    * lists are usually tiny anyway. */
   list = kcalloc(NicAddressList_length(nicList), sizeof(*list), GFP_NOFS | __GFP_NOFAIL);
   p = list;

   NicAddressListIter_init(&listIter, nicList);

   while (!NicAddressListIter_end(&listIter)) {
      p->preferences = preferences;
      p->addr = NicAddressListIter_value(&listIter);

      NicAddressListIter_next(&listIter);
      p++;
   }

   sort(list, NicAddressList_length(nicList), sizeof(*list), nicaddr_sort_comp, NULL);

   /* *maybe* the clone already contains elements, don't rely on its size */
   p = list;
   NicAddressList_init(nicListClone);
   while (NicAddressList_length(nicList) != NicAddressList_length(nicListClone)) {
      NicAddress* clone = os_kmalloc(sizeof(*clone));
      memcpy(clone, p->addr, sizeof(*clone));
      NicAddressList_append(nicListClone, clone);
      p++;
   }

   kfree(list);
}

void ListTk_kfreeNicAddressListElems(NicAddressList* nicList)
{
   NicAddressListIter nicIter;

   NicAddressListIter_init(&nicIter, nicList);

   for( ; !NicAddressListIter_end(&nicIter); NicAddressListIter_next(&nicIter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
      kfree(nicAddr);
   }
}

void ListTk_kfreeNicAddressStatsListElems(NicAddressStatsList* nicStatsList)
{
   NicAddressStatsListIter nicStatsIter;

   NicAddressStatsListIter_init(&nicStatsIter, nicStatsList);

   for( ; !NicAddressStatsListIter_end(&nicStatsIter); NicAddressStatsListIter_next(&nicStatsIter) )
   {
      NicAddressStats* nicStats = NicAddressStatsListIter_value(&nicStatsIter);
      NicAddressStats_uninit(nicStats);
      kfree(nicStats);
   }
}

void ListTk_copyUInt16ListToVec(UInt16List* srcList, UInt16Vec* destVec)
{
   UInt16ListIter listIter;

   UInt16ListIter_init(&listIter, srcList);

   for( ; !UInt16ListIter_end(&listIter); UInt16ListIter_next(&listIter) )
   {
      int currentElem = UInt16ListIter_value(&listIter);

      UInt16Vec_append(destVec, currentElem);
   }
}


/**
 * @param outPos zero-based list position of the searchStr if found, undefined otherwise
 */
bool ListTk_listContains(char* searchStr, StrCpyList* list, ssize_t* outPos)
{
   StrCpyListIter iter;

   (*outPos) = 0;

   StrCpyListIter_init(&iter, list);

   for( ; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter) )
   {
      char* currentElem = StrCpyListIter_value(&iter);

      if(!strcmp(searchStr, currentElem) )
         return true;

      (*outPos)++;
   }

   (*outPos) = -1;

   return false;
}



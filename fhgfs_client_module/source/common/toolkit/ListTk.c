#include <common/nodes/ConnectionListIter.h>
#include <common/net/sock/NicAddressListIter.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>
#include "ListTk.h"


void ListTk_cloneNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone)
{
   NicAddressListIter iter;

   NicAddressList_init(nicListClone);

   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);
      NicAddress* nicAddrClone;

      // clone element

      nicAddrClone = (NicAddress*)os_kmalloc(sizeof(NicAddress) );

      memcpy(nicAddrClone, nicAddr, sizeof(NicAddress) );

      // append to the clone list
      NicAddressList_append(nicListClone, nicAddrClone);
   }
}

/**
 * Returns a cloned version of a list with only standard NICs in it.
 * (Non-standard elements in the source will just be ignored.)
 */
void ListTk_cloneStandardNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone)
{
   NicAddressListIter iter;

   NicAddressList_init(nicListClone);

   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);
      NicAddress* nicAddrClone;

      // ignore non-standard NICs
      if(nicAddr->nicType != NICADDRTYPE_STANDARD)
         continue;

      // clone
      nicAddrClone = (NicAddress*)os_kmalloc(sizeof(NicAddress) );
      memcpy(nicAddrClone, nicAddr, sizeof(NicAddress) );

      // append to the clone list
      NicAddressList_append(nicListClone, nicAddrClone);
   }
}


/**
 * Returns a sorted clone of a nicList.
 *
 * Note: As this is based on a tree insertion, it won't work when there are duplicate elements
 * in the list.
 */
void ListTk_cloneSortNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone)
{
   NicAddressListIter listIter;
   RBTree tree;
   RBTreeIter treeIter;


   PointerRBTree_init(&tree, NicAddress_treeComparator);

   // sort => insert each list elem into the tree (no copying)

   NicAddressListIter_init(&listIter, nicList);

   for( ; !NicAddressListIter_end(&listIter); NicAddressListIter_next(&listIter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&listIter);

      // Note: We insert elems into the tree without copying, so there's no special cleanup
      //    required for the tree when we're done
      PointerRBTree_insert(&tree, nicAddr, NULL);
   }


   // copy elements from tree to clone list (sorted)

   NicAddressList_init(nicListClone);

   treeIter = PointerRBTree_begin(&tree);

   for( ; !PointerRBTreeIter_end(&treeIter); PointerRBTreeIter_next(&treeIter) )
   {
      NicAddress* nicAddr = (NicAddress*)PointerRBTreeIter_key(&treeIter);

      // clone
      NicAddress* nicAddrClone = (NicAddress*)os_kmalloc(sizeof(NicAddress) );
      memcpy(nicAddrClone, nicAddr, sizeof(NicAddress) );

      // append to the clone list
      NicAddressList_append(nicListClone, nicAddrClone);
   }

   PointerRBTree_uninit(&tree);
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

void ListTk_copyStrCpyListToVec(StrCpyList* srcList, StrCpyVec* destVec)
{
   StrCpyListIter listIter;

   StrCpyListIter_init(&listIter, srcList);

   for( ; !StrCpyListIter_end(&listIter); StrCpyListIter_next(&listIter) )
   {
      char* currentElem = StrCpyListIter_value(&listIter);

      StrCpyVec_append(destVec, currentElem);
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



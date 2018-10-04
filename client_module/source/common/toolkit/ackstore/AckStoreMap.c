#include "AckStoreMap.h"
#include "AckStoreMapIter.h"

AckStoreMapIter AckStoreMap_find(AckStoreMap* this, const char* searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem( (RBTree*)this, searchKey);

   AckStoreMapIter iter;
   AckStoreMapIter_init(&iter, this, treeElem);

   return iter;
}

int compareAckStoreMapElems(const void* key1, const void* key2)
{
   return strcmp(key1, key2);
}




#include "WaitAckMap.h"
#include "WaitAckMapIter.h"

WaitAckMapIter WaitAckMap_find(WaitAckMap* this, const char* searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem( (RBTree*)this, searchKey);
   
   WaitAckMapIter iter;
   WaitAckMapIter_init(&iter, this, treeElem);

   return iter;
}

WaitAckMapIter WaitAckMap_begin(WaitAckMap* this)
{
   RBTreeElem* treeElem = container_of(rb_first(&this->rbTree.treeroot), RBTreeElem, treenode);

   WaitAckMapIter iter;
   WaitAckMapIter_init(&iter, this, treeElem);

   return iter;
}


int compareWaitAckMapElems(const void* key1, const void* key2)
{
   return strcmp(key1, key2);
}




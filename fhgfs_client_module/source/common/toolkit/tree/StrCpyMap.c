#include "StrCpyMap.h"
#include "StrCpyMapIter.h"

StrCpyMapIter StrCpyMap_find(StrCpyMap* this, const char* searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem( (RBTree*)this, searchKey);

   StrCpyMapIter iter;
   StrCpyMapIter_init(&iter, this, treeElem);

   return iter;
}

StrCpyMapIter StrCpyMap_begin(StrCpyMap* this)
{
   struct rb_node* node = rb_first(&this->rbTree.treeroot);
   RBTreeElem* treeElem = node ? container_of(node, RBTreeElem, treenode) : NULL;

   StrCpyMapIter iter;
   StrCpyMapIter_init(&iter, this, treeElem);

   return iter;
}


int compareStrCpyMapElems(const void* key1, const void* key2)
{
   return strcmp(key1, key2);
}


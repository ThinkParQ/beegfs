#include <common/toolkit/tree/UInt16Map.h>
#include <common/toolkit/tree/UInt16MapIter.h>


UInt16MapIter UInt16Map_find(UInt16Map* this, const uint16_t searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem(
      (RBTree*)this, (const void*)(const size_t)searchKey);

   UInt16MapIter iter;
   UInt16MapIter_init(&iter, this, treeElem);

   return iter;
}

UInt16MapIter UInt16Map_begin(UInt16Map* this)
{
   RBTreeElem* treeElem = container_of(rb_first(&this->rbTree.treeroot), RBTreeElem, treenode);

   UInt16MapIter iter;
   UInt16MapIter_init(&iter, this, treeElem);

   return iter;
}

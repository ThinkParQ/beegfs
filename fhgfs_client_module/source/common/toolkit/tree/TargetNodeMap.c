#include <common/toolkit/tree/TargetNodeMap.h>
#include <common/toolkit/tree/TargetNodeMapIter.h>


TargetNodeMapIter TargetNodeMap_find(TargetNodeMap* this, const uint16_t searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem(
      (RBTree*)this, (const void*)(const size_t)searchKey);

   TargetNodeMapIter iter;
   TargetNodeMapIter_init(&iter, this, treeElem);

   return iter;
}

TargetNodeMapIter TargetNodeMap_begin(TargetNodeMap* this)
{
   RBTreeElem* treeElem = container_of(rb_first(&this->rbTree.treeroot), RBTreeElem, treenode);

   TargetNodeMapIter iter;
   TargetNodeMapIter_init(&iter, this, treeElem);

   return iter;
}

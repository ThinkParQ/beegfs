#include <common/toolkit/tree/TargetStateInfoMap.h>
#include <common/toolkit/tree/TargetStateInfoMapIter.h>

TargetStateInfoMapIter TargetStateInfoMap_find(TargetStateInfoMap* this,
   const uint16_t searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem(
      (RBTree*)this, (const void*)(const size_t)searchKey);

   TargetStateInfoMapIter iter;
   TargetStateInfoMapIter_init(&iter, this, treeElem);

   return iter;
}

TargetStateInfoMapIter TargetStateInfoMap_begin(TargetStateInfoMap* this)
{
   struct rb_node* node = rb_first(&this->rbTree.treeroot);
   RBTreeElem* treeElem = node ? container_of(node, RBTreeElem, treenode) : NULL;

   TargetStateInfoMapIter iter;
   TargetStateInfoMapIter_init(&iter, this, treeElem);

   return iter;
}

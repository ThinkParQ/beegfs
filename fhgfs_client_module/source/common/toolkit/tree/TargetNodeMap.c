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
   struct rb_node* node = rb_first(&this->rbTree.treeroot);
   RBTreeElem* treeElem = node ? container_of(node, RBTreeElem, treenode) : NULL;

   TargetNodeMapIter iter;
   TargetNodeMapIter_init(&iter, this, treeElem);

   return iter;
}

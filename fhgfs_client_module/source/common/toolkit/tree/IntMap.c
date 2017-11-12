#include <common/toolkit/tree/IntMap.h>
#include <common/toolkit/tree/IntMapIter.h>

IntMapIter IntMap_find(IntMap* this, const int searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem(
      (RBTree*)this, (const void*)(const size_t)searchKey);

   IntMapIter iter;
   IntMapIter_init(&iter, this, treeElem);

   return iter;
}

IntMapIter IntMap_begin(IntMap* this)
{
   struct rb_node* node = rb_first(&this->rbTree.treeroot);
   RBTreeElem* treeElem = node ? container_of(node, RBTreeElem, treenode) : NULL;

   IntMapIter iter;
   IntMapIter_init(&iter, this, treeElem);

   return iter;
}

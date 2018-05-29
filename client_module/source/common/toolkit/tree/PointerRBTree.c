#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>


RBTreeIter PointerRBTree_find(RBTree* this, const void* searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem(this, searchKey);

   RBTreeIter iter;
   PointerRBTreeIter_init(&iter, this, treeElem);

   return iter;
}

RBTreeIter PointerRBTree_begin(RBTree* this)
{
   struct rb_node* node = rb_first(&this->treeroot);
   RBTreeElem* treeElem = node ? container_of(node, RBTreeElem, treenode) : NULL;

   RBTreeIter iter;
   PointerRBTreeIter_init(&iter, this, treeElem);

   return iter;
}

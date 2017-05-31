#include <common/toolkit/tree/MirrorBuddyGroupMap.h>
#include <common/toolkit/tree/MirrorBuddyGroupMapIter.h>

MirrorBuddyGroupMapIter MirrorBuddyGroupMap_find(MirrorBuddyGroupMap* this,
   const uint16_t searchKey)
{
   RBTreeElem* treeElem = _PointerRBTree_findElem(
      (RBTree*)this, (const void*)(const size_t)searchKey);

   MirrorBuddyGroupMapIter iter;
   MirrorBuddyGroupMapIter_init(&iter, this, treeElem);

   return iter;
}

MirrorBuddyGroupMapIter MirrorBuddyGroupMap_begin(MirrorBuddyGroupMap* this)
{
   struct rb_node* node = rb_first(&this->rbTree.treeroot);
   RBTreeElem* treeElem = node ? container_of(node, RBTreeElem, treenode) : NULL;

   MirrorBuddyGroupMapIter iter;
   MirrorBuddyGroupMapIter_init(&iter, this, treeElem);

   return iter;
}

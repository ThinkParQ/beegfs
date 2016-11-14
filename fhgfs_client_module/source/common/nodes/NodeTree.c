#include "NodeTree.h"

#include <os/OsCompat.h>

void NodeTree_init(NodeTree* this)
{
   this->nodes = RB_ROOT;
   this->size = 0;
}

void NodeTree_uninit(NodeTree* this)
{
   Node* node;
   Node* tmp;

   // delete activeNodes. frees all memory, but leaves the rbtree in the shape it was - after this,
   // any operation on the rbtree is undefined.
   rbtree_postorder_for_each_entry_safe(node, tmp, &this->nodes, _nodeTree.rbTreeElement)
      Node_put(node);
}

Node* NodeTree_find(NodeTree* this, NumNodeID nodeNumID)
{
   struct rb_node* node = this->nodes.rb_node;

   while(node)
   {
      Node* elem = rb_entry(node, Node, _nodeTree.rbTreeElement);
      NumNodeID elemID = Node_getNumID(elem);

      if(nodeNumID.value < elemID.value)
         node = node->rb_left;
      else
      if(nodeNumID.value > elemID.value)
         node = node->rb_right;
      else
         return elem;
   }

   return NULL;
}

Node* NodeTree_getNext(NodeTree* this, Node* node)
{
   struct rb_node* rbNode = rb_next(&node->_nodeTree.rbTreeElement);

   if(!rbNode)
      return NULL;

   return rb_entry(rbNode, Node, _nodeTree.rbTreeElement);
}

bool NodeTree_insert(NodeTree* this, NumNodeID nodeNumID, Node* node)
{
   struct rb_node** new = &this->nodes.rb_node;
   struct rb_node* parent = NULL;

   while(*new)
   {
      Node* elem = rb_entry(*new, Node, _nodeTree.rbTreeElement);
      NumNodeID elemID = Node_getNumID(elem);

      parent = *new;
      if(nodeNumID.value < elemID.value)
         new = &(*new)->rb_left;
      else
      if(nodeNumID.value > elemID.value)
         new = &(*new)->rb_right;
      else
         return false;
   }

   rb_link_node(&node->_nodeTree.rbTreeElement, parent, new);
   rb_insert_color(&node->_nodeTree.rbTreeElement, &this->nodes);
   this->size++;

   return true;
}

bool NodeTree_erase(NodeTree* this, NumNodeID nodeNumID)
{
   Node* node;

   node = NodeTree_find(this, nodeNumID);
   if(node)
   {
      rb_erase(&node->_nodeTree.rbTreeElement, &this->nodes);
      this->size--;
      Node_put(node);
      return true;
   }

   return false;
}

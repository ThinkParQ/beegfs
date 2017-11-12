package com.beegfs.admon.gui.common.nodes;

import com.beegfs.admon.gui.common.enums.NodeTypesEnum;

public class TypedNodes extends Nodes
{
   private final NodeTypesEnum nodeType;

   public TypedNodes(NodeTypesEnum nodeType)
   {
      super();
      this.nodeType = nodeType;
   }

   public NodeTypesEnum getNodeType()
   {
      return this.nodeType;
   }

   public boolean contains(String nodeID)
   {
      return super.contains(nodeID, nodeType);
   }

   public boolean contains(int nodeNumID)
   {
      return super.contains(nodeNumID, nodeType);
   }

   public Node getNode(String nodeID)
   {
      return super.getNode(nodeID, nodeType);
   }

   public Node getNode(int nodeNumID)
   {
      return super.getNode(nodeNumID, nodeType);
   }

   public Nodes getNodes(String group)
   {
      return super.getNodes(group, nodeType);
   }

   public Nodes getClonedNodes(String group) throws CloneNotSupportedException
   {
      return super.getClonedNodes(group, nodeType);
   }

   @Override
   public boolean add(Node node)
   {
      if (this.nodeType != node.getType())
      {
         return false;
      }

      return super.add(node);
   }

   @Override
   public boolean addNodes(Nodes nodeList)
   {
      for(Node node : nodeList)
      {
         if (this.nodeType != node.getType())
         {
            return false;
         }

         return super.add(node);
      }

      return true;
   }

   public boolean remove(String nodeID)
   {
      return super.remove(nodeID, nodeType);
   }

   public boolean remove(int nodeNumID)
   {
      return super.remove(nodeNumID, nodeType);
   }
}

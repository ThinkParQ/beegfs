package com.beegfs.admon.gui.common;


import com.beegfs.admon.gui.common.nodes.Node;
import com.beegfs.admon.gui.common.nodes.Nodes;



public class Group
{
   private final String name;
   private final Nodes nodes;

   public Group(String name)
   {
      this.name = name;
      nodes = new Nodes();
   }

   public void addNode(Node node)
   {
      nodes.add(node);
   }

   public void addNodes(Nodes nodes)
   {
      for (Node node : nodes)
      {
         nodes.add(node);
      }
   }

   public void delNode(Node node)
   {
      nodes.remove(node);
   }

   public void delAll()
   {
      nodes.clear();
   }

   public final String getName()
   {
      return name;
   }

   public Nodes getNodes()
   {
      return nodes;
   }

}

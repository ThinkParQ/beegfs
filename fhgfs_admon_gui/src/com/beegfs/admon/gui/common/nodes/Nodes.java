package com.beegfs.admon.gui.common.nodes;

import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;


public class Nodes implements Iterable<Node>
{
   protected final static int INITIAL_CAPACITY = 10;
   protected final List<Node> nodes;

   public Nodes()
   {
      nodes = Collections.synchronizedList(new ArrayList<Node>(INITIAL_CAPACITY));
   }

   public boolean contains(Node node)
   {
      return nodes.contains(node);
   }

   public boolean contains(String nodeID, NodeTypesEnum type)
   {
      synchronized(nodes)
      {
         for (Node tmpNode : nodes)
         {
            if (type == tmpNode.getType() && nodeID.equals(tmpNode.getNodeID()))
            {
               return true;
            }
         }
      }
      return false;
   }

   public boolean contains(int nodeNumID, NodeTypesEnum type)
   {
      synchronized(nodes)
      {
         for (Node tmpNode : nodes)
         {
            if (type == tmpNode.getType() && nodeNumID == tmpNode.getNodeNumID())
            {
               return true;
            }
         }
      }
      return false;
   }

   public Node getNode(String nodeID, NodeTypesEnum type)
   {
      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (type == node.getType() && node.getNodeID().equals(nodeID))
            {
               return node;
            }
         }
      }
      return null;
   }

   public Node getNode(int nodeNumID, NodeTypesEnum type)
   {
      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (type == node.getType() && node.getNodeNumID() == nodeNumID)
            {
               return node;
            }
         }
      }
      return null;
   }

   public Nodes getNodes(String group, NodeTypesEnum type)
   {
      Nodes groupNodes = new Nodes();

      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (type == node.getType() && node.getGroup().equals(group))
            {
               groupNodes.add(node);
            }
         }
      }
      return groupNodes;
   }

   public Nodes getClonedNodes(String group, NodeTypesEnum type) throws CloneNotSupportedException
   {
      Nodes groupNodes = new Nodes();

      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (type == node.getType() && node.getGroup().equals(group))
            {
               groupNodes.add((Node)node.clone());
            }
         }
      }
      return groupNodes;
   }

   public boolean add(Node node)
   {
      if (!nodes.contains(node))
      {
         return nodes.add(node);
      }
      else
      {
         return true;
      }
   }

   public boolean addNodes(Nodes nodeList)
   {
      boolean retVal = true;
      boolean oneFound = false;

      synchronized(nodeList)
      {
         for(Node node : nodeList)
         {
            if (this.add(node))
            {
               oneFound = true;
            }
            else
            {
               retVal = false;
               oneFound = true;
            }
         }
      }

      if (!oneFound)
      {
         retVal = false;
      }

      return retVal;
   }

   public boolean remove(Node node)
   {
      return nodes.remove(node);
   }

   public boolean remove(String nodeID, NodeTypesEnum type)
   {
      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (type  == node.getType() && node.getNodeID().equals(nodeID))
            {
               return this.remove(node);
            }
         }
      }
      return false;
   }

   public boolean remove(int nodeNumID, NodeTypesEnum type)
   {
      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (type  == node.getType() && node.getNodeNumID() == nodeNumID)
            {
               return this.remove(node);
            }
         }
      }
      return false;
   }

   public boolean removeNodes(String group)
   {
      boolean retVal = true;
      boolean oneFound = false;

      synchronized(nodes)
      {
         for (Node node : nodes)
         {
            if (node.getGroup().equals(group))
            {
               if (this.remove(node))
               {
                  oneFound = true;
               }
               else
               {
                  retVal = false;
                  oneFound = true;
               }
            }
         }
      }

      if (!oneFound)
      {
         retVal = false;
      }

      return retVal;
   }

   public boolean removeNodes(Nodes nodeList)
   {
      boolean retVal = true;
      boolean oneFound = false;

      synchronized(nodeList)
      {
         for (Node node : nodeList)
         {
            if (this.remove(node))
            {
               oneFound = true;
            }
            else
            {
               retVal = false;
               oneFound = true;
            }
         }
      }

      if (!oneFound)
      {
         retVal = false;
      }

      return retVal;
   }

   public int syncNodes(Nodes nodeList)
   {
      int counter = 0;

      synchronized(nodeList)
      {
         for (Node node : nodeList)
         {
            if(!this.contains(node))
            {
               if(!this.add(node))
               {
                  return -1;
               }
               counter++;
            }
         }
      }

      return counter;
   }

   public void clear()
   {
      nodes.clear();
   }

   @Override
   public Iterator<Node> iterator()
   {
      return nodes.iterator();
   }

   public int size()
   {
      return nodes.size();
   }
}

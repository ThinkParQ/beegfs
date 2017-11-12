package com.beegfs.admon.gui.common.nodes;

import com.beegfs.admon.gui.common.enums.NodeTypesEnum;

public class Node implements Cloneable
{
   public final static String DEFAULT_GROUP = "Default";

   public static String getNodeIDFromTypedNodeID(String typedNodeID)
   {
      String[] splitedID = typedNodeID.split(" ");
      if(splitedID.length > 0)
      {
         return splitedID[0];
      }
      else
      {
         return "";
      }
   }

   public static int getNodeNumIDFromTypedNodeID(String typedNodeID) throws NumberFormatException
   {
      String[] splitedID = typedNodeID.split(" ");
      if(splitedID.length > 2)
      {
         return Integer.parseInt(splitedID[2].replace("]", ""));
      }
      else
      {
         return 0;
      }
   }

   public static String getTypedNodeID(String nodeID, int nodeNumID, NodeTypesEnum type)
   {
      if (type == NodeTypesEnum.CLIENT)
      {
         return nodeID;
      }
      else
      {
         return nodeID + " [ID: " + String.valueOf(nodeNumID) + "]";
      }
   }

   private int nodeNumID;
   private String nodeID;
   private String group;
   private NodeTypesEnum type;

   public Node(int nodeNumID, String nodeID, String group, NodeTypesEnum type)
   {
      this.nodeNumID = nodeNumID;
      this.nodeID = nodeID;
      this.group = group;
      this.type = type;
   }

   public Node(int nodeNumID, String nodeID, NodeTypesEnum type)
   {
      this.nodeNumID = nodeNumID;
      this.nodeID = nodeID;
      this.group = DEFAULT_GROUP;
      this.type = type;
   }

   public Node(String typedNodeID, NodeTypesEnum type)
   {
      String[] splitedID = typedNodeID.split(" ");
      this.nodeID = splitedID[0];
      this.nodeNumID = Integer.parseInt(splitedID[2].replace("]", ""));
      this.group = DEFAULT_GROUP;
      this.type = type;
   }

   public Node(String typedNodeID, String group, NodeTypesEnum type)
   {
      String[] splitedID = typedNodeID.split(" ");
      this.nodeID = splitedID[0];
      this.nodeNumID = Integer.parseInt(splitedID[2].replace("]", ""));
      this.group = group;
      this.type = type;
   }

   public String getNodeID()
   {
      return this.nodeID;
   }

   public int getNodeNumID()
   {
      return this.nodeNumID;
   }

   public String getGroup()
   {
      return this.group;
   }

   public NodeTypesEnum getType()
   {
      return this.type;
   }

   public void setNodeID(String nodeID)
   {
      this.nodeID = nodeID;
   }

   public void setNodeNumID(int nodeNumID)
   {
      this.nodeNumID = nodeNumID;
   }

   public void setGroup(String group)
   {
      this.group = group;
   }

   public void setType(NodeTypesEnum type)
   {
      this.type = type;
   }

   public String getTypedNodeID()
   {
      if (type == NodeTypesEnum.CLIENT)
      {
         return nodeID;
      }
      else
      {
         return nodeID + " [ID: " + String.valueOf(nodeNumID) + "]";
      }
   }

   @Override
   public boolean equals(Object obj)
   {
      Node node;

      if (obj == null)
      {
         return false;
      }

      if (obj instanceof Node)
      {
         node = (Node) obj;
      }
      else
      {
         return false;
      }

      if (nodeNumID != node.getNodeNumID())
      {
         return false;
      }

      if (type != node.getType())
      {
         return false;
      }

      if (type != node.getType())
      {
         return false;
      }

      return nodeID.equals(node.getNodeID());
   }

   @Override
   public int hashCode()
   {
      int hash = 3;
      hash = 67 * hash + this.nodeNumID;
      hash = 67 * hash + (this.nodeID != null ? this.nodeID.hashCode() : 0);
      hash = 67 * hash + (this.group != null ? this.group.hashCode() : 0);
      hash = 67 * hash + (this.type != null ? this.type.hashCode() : 0);
      return hash;
   }

   @Override
   public Object clone() throws CloneNotSupportedException
   {
      return super.clone();
   }
}
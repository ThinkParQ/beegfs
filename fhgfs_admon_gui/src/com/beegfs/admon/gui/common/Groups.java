package com.beegfs.admon.gui.common;

import com.beegfs.admon.gui.common.nodes.Nodes;
import java.util.ArrayList;

public class Groups
{
   private final static int INITIAL_CAPACITY = 10;
   private final ArrayList<Group> groups;

   public Groups()
   {
      groups = new ArrayList<>(INITIAL_CAPACITY);
   }

   public Group getGroup(String name)
   {
      Group retVal = null;
      for (Group group : groups)
      {
         if (group.getName().equals(name))
         {
            retVal = group;
            break;
         }
      }
      if (retVal == null)
      {
         Group newGroup = new Group(name);
         groups.add(newGroup);
         retVal = newGroup;
      }
      return retVal;
   }

   public Group getDefaultGroup()
   {
      return getGroup("Default");
   }

   public ArrayList<String> getGroupNames()
   {
      ArrayList<String> names = new ArrayList<>(groups.size());
      for (Group group : groups)
      {
         names.add(group.getName());
      }
      return names;
   }

   public void clear()
   {
      groups.clear();
   }

   public void removeGroup(String name)
   {
      Group group = getGroup(name);
      Group defaultGroup = getDefaultGroup();
      Nodes nodes = group.getNodes();
      defaultGroup.addNodes(nodes);
      groups.remove(group);
   }

   public boolean isDefault(String groupName)
   {
      return getDefaultGroup().getName().equals(groupName);
   }
}

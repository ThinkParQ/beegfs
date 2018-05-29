package com.beegfs.admon.gui.components.tables;

public class FilenameObject implements Comparable<FilenameObject>
{
   private final String name;
   private final boolean isDir;

   public FilenameObject()
   {
      name = "";
      isDir = false;
   }

   public FilenameObject(String name, boolean isDir)
   {
      this.name = name;
      this.isDir = isDir;
   }

   public String getName()
   {
      return this.name;
   }

   public boolean isDirectory()
   {
      return this.isDir;
   }

   @Override
   public int compareTo(FilenameObject o)
   {
      if (this.name.compareTo("..") == 0)
      {
         return -1;
      }
      if (o.getName().compareTo("..") == 0)
      {
         return 1;
      }
      else
      if (this.isDir && o.isDirectory())
      {
         return this.name.compareToIgnoreCase(o.getName());
      }
      else
      if(this.isDir)
      {
         return -1;
      }
      else
      if (o.isDirectory())
      {
         return 1;
      }
      else
      {
         return this.name.compareToIgnoreCase(o.getName());
      }
   }

   @Override
   public boolean equals(Object anObject)
   {
      if (anObject == null)
      {
         return false;
      }
      if (anObject == this)
      {
         return true;
      }
      if (!(anObject instanceof FilenameObject))
      {
         return false;
      }

      FilenameObject o = (FilenameObject) anObject;

      return this.isDir == o.isDirectory() && this.name.equals(o.getName());
   }

   @Override
   public int hashCode()
   {
      int hash = 7;
      hash = 71 * hash + (this.name != null ? this.name.hashCode() : 0);
      hash = 71 * hash + (this.isDir ? 1 : 0);
      return hash;
   }
}

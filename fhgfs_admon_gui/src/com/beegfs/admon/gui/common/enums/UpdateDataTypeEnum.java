package com.beegfs.admon.gui.common.enums;

import com.beegfs.admon.gui.program.Main;

public enum UpdateDataTypeEnum
{
   HOSTS_MGMTD(Main.getLocal().getString("Add management hosts"), Main.getLocal().getString("Import management hosts from file"),
      Main.getLocal().getString("Please choose the file to import.") + System.lineSeparator() + System.lineSeparator() +
      Main.getLocal().getString("The file must contain one host per line.")),
   HOSTS_META(Main.getLocal().getString("Add metadata hosts"), Main.getLocal().getString("Import metadata hosts from file"),
      Main.getLocal().getString("Please choose the file to import.") + System.lineSeparator() + System.lineSeparator() +
      Main.getLocal().getString("The file must contain one host per line.")),
   HOSTS_STORAGE(Main.getLocal().getString("Add storage hosts"), Main.getLocal().getString("Import storage hosts from file"),
      Main.getLocal().getString("Please choose the file to import.") + System.lineSeparator() + System.lineSeparator() +
      Main.getLocal().getString("The file must contain one host per line.")),
   HOSTS_CLIENT(Main.getLocal().getString("Add client hosts"), Main.getLocal().getString("Import client hosts from file"),
      Main.getLocal().getString("Please choose the file to import.") + System.lineSeparator() + System.lineSeparator() +
      Main.getLocal().getString("The file must contain one host per line.")),
   QUOTA(Main.getLocal().getString("Add quota limits"), Main.getLocal().getString("Import quota limits from file"),
      Main.getLocal().getString("Please choose the file to import.") + System.lineSeparator() + System.lineSeparator() +
      Main.getLocal().getString("A csv file is requires each line is a user or a group.") + System.lineSeparator() +
      Main.getLocal().getString("Line format: ID/name,size limit,inode limit"));

   private final String title;
   private final String fileCooserTitle;
   private final String description;

   UpdateDataTypeEnum(String title, String fileCooserTitle, String description)
   {
      this.title = title;
      this.fileCooserTitle = fileCooserTitle;
      this.description = description;
   }

   public String getTitle()
   {
      return title;
   }

   public String getFileChooserTitle()
   {
      return fileCooserTitle;
   }

   public String getDescription()
   {
      return description;
   }
}

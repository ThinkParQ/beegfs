package com.beegfs.admon.gui.common.enums;


public enum UpdateDataTypeEnum
{
   HOSTS_MGMTD("Add management hosts", "Import management hosts from file",
      "Please choose the file to import." + System.lineSeparator() + System.lineSeparator() +
      "The file must contain one host per line."),
   HOSTS_META("Add metadata hosts", "Import metadata hosts from file",
      "Please choose the file to import." + System.lineSeparator() + System.lineSeparator() +
      "The file must contain one host per line."),
   HOSTS_STORAGE("Add storage hosts", "Import storage hosts from file",
      "Please choose the file to import." + System.lineSeparator() + System.lineSeparator() +
      "The file must contain one host per line."),
   HOSTS_CLIENT("Add client hosts", "Import client hosts from file",
      "Please choose the file to import." + System.lineSeparator() + System.lineSeparator() +
      "The file must contain one host per line."),
   QUOTA("Add quota limits", "Import quota limits from file",
      "Please choose the file to import." + System.lineSeparator() + System.lineSeparator() +
      "A csv file is requires each line is a user or a group." + System.lineSeparator() +
      "Line format: ID/name,size limit,inode limit");

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

package com.beegfs.admon.gui.common.enums;



public enum FilePathsEnum
{
   DEFAULT_CONFIG_FILE(System.getProperty(PropertyEnum.PROPERTY_HOME_DIR.getKey()) +
      "/.beegfs/beegfsGUI.config"),
   DEFAULT_LOG_FILE(System.getProperty(PropertyEnum.PROPERTY_HOME_DIR.getKey()) +
      "/.beegfs/beegfs_admon_gui.log"),
   DIRECTORY(System.getProperty(PropertyEnum.PROPERTY_HOME_DIR.getKey()) + "/.beegfs"),
   EULA(FilePathsEnum.FILES_PATH + "/BeeGFS_EULA"),
   IMAGE_BEEGFS_LOGO(FilePathsEnum.IMAGES_PATH + "/beegfs-logo.png"),
   IMAGE_BEEGFS_ICON(FilePathsEnum.IMAGES_PATH + "/beegfs.ico"),
   IMAGE_INFO(FilePathsEnum.IMAGES_PATH + "/info.png"),
   IMAGE_LEAF_ICON(FilePathsEnum.IMAGES_PATH + "/leafIcon.jpg"),
   IMAGE_STATUS_FAIL(FilePathsEnum.IMAGES_PATH + "/redBall.png"),
   IMAGE_STATUS_OK(FilePathsEnum.IMAGES_PATH + "/greenBall.png");


   private static final String RESOURCES_PATH = "/resources";
   private static final String FILES_PATH = RESOURCES_PATH + "/files";
   private static final String IMAGES_PATH = RESOURCES_PATH + "/images";

   private final String path;

   FilePathsEnum(String path)
   {
      this.path = path;
   }

   public String getPath()
   {
      return path;
   }

}

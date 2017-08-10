package com.beegfs.admon.gui.common.enums;


import static com.beegfs.admon.gui.common.tools.DefinesTk.BEEGFS_ADMON_GUI_NAMESPACE;



public enum PropertyEnum
{
   PROPERTY_ADMON_GUI_CFG_FILE(BEEGFS_ADMON_GUI_NAMESPACE + ".cfgFile"),
   PROPERTY_ADMON_GUI_HELP(BEEGFS_ADMON_GUI_NAMESPACE + ".help"),
   PROPERTY_ADMON_GUI_IGNORE_VERSION_TEST(BEEGFS_ADMON_GUI_NAMESPACE + ".ignoreVersionTest"),
   PROPERTY_ADMON_GUI_LOG_DBG(BEEGFS_ADMON_GUI_NAMESPACE + ".logDebug"), // do not rename to DEBUG, the ant script destroys the main source file
   PROPERTY_ADMON_GUI_LOG_FILE(BEEGFS_ADMON_GUI_NAMESPACE + ".logFile"),
   PROPERTY_ADMON_GUI_LOG_LEVEL(BEEGFS_ADMON_GUI_NAMESPACE + ".logLevel"),
   PROPERTY_ADMON_GUI_LOCAL(BEEGFS_ADMON_GUI_NAMESPACE + ".local"),
   PROPERTY_ADMON_GUI_RESOLUTION(BEEGFS_ADMON_GUI_NAMESPACE + ".resolution"),
   PROPERTY_ADMON_HOST(BEEGFS_ADMON_GUI_NAMESPACE + ".admonHost"),
   PROPERTY_ADMON_HTTP_PORT(BEEGFS_ADMON_GUI_NAMESPACE + ".admonHttpPort"),
   PROPERTY_APPLE_MENU_ABOUT_NAME("com.apple.mrj.application.apple.menu.about.name"),
   PROPERTY_APPLE_SCREEN_MENU_BAR("apple.laf.useScreenMenuBar"),
   PROPERTY_HOME_DIR("user.home"),
   PROPERTY_JAVA_VERSION("java.version"),
   PROPERTY_JAVA2D_OFFSCREEN("sun.java2d.pmoffscreen"),
   PROPERTY_LINE_SEPARATOR("line.separator"),
   PROPERTY_OS_NAME("os.name");
   

   PropertyEnum(String key)
   {
      this.key = key;
   }

   private final String key;

   public String getKey()
   {
      return this.key;
   }

   public String getConfigKey()
   {
      if(this.key.startsWith(BEEGFS_ADMON_GUI_NAMESPACE))
      {
         return this.key.substring(this.key.lastIndexOf('.') + 1);
      }
      else
      {
         return this.key;
      }
   }

   public String getArgsKey()
   {
      return "--" + getConfigKey();
   }
}

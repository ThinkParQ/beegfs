package com.beegfs.admon.gui.program;

import com.beegfs.admon.gui.app.config.Config;
import com.beegfs.admon.gui.app.log.InternalLogger;
import com.beegfs.admon.gui.common.Session;
import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.common.enums.PropertyEnum;
import static com.beegfs.admon.gui.common.tools.DefinesTk.BEEGFS_ADMON_GUI_NAMESPACE;
import com.beegfs.admon.gui.components.MainWindow;
import com.beegfs.admon.gui.components.dialogs.JDialogNewConfig;
import com.beegfs.admon.gui.components.lf.BeegfsLookAndFeel;
import java.awt.HeadlessException;
import java.io.File;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.naming.ConfigurationException;
import javax.swing.JOptionPane;
import javax.swing.UIManager;
import javax.swing.UnsupportedLookAndFeelException;


public class Main
{ 
   private static InternalLogger logger;

   private static MainWindow mainWindow;
   private static Config config;
   private static Session session;
   private static final double JAVA_MIN_VERSION = 1.7;
   private static final String BEEGFS_VERSION = "@VERSION@";

   /**
    * @param args the command line arguments
    */
   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   public static void main(String args[])
   {
      // try to create the BeeGFS directory if not present
      File f = new File(FilePathsEnum.DIRECTORY.getPath());
      if (!f.exists() )
      {
         if(!f.mkdir() )
         {
            System.err.println("Could not create BeeGFS admon GUI directory: " +
               FilePathsEnum.DIRECTORY.getPath());

            System.exit(1);
         }
      }

      resetRootLogger();

      try
      {
         //parse args
         config = new Config(args);
      }
      catch (ConfigurationException ex)
      {
         System.err.println(ex.getMessage());
         System.exit(1);
      }

      initLogger();

      if(!checkJavaVersion())
      {
         System.exit(1);
      }

      if(!checkOsAndPrepare())
      {
         System.exit(1);
      }

      if(initConfigFromFile())
      {
         //create session
         session = new Session();
         mainWindow = new MainWindow();

         java.awt.EventQueue.invokeLater(new Runnable()
         {
            @Override
            public void run()
            {
               //    create the GUI
               mainWindow.setVisible(true);
               mainWindow.getLoginDialog().setVisible(true);
            }
         });
      }
      else
      {
         System.exit(1);
      }
   }

   public static MainWindow getMainWindow()
   {
      return Main.mainWindow;
   }

   public static Config getConfig()
   {
      return Main.config;
   }

   public static Session getSession()
   {
      return Main.session;
   }

   public static String getVersion()
   {
      return Main.BEEGFS_VERSION;
   }

   public static void setInternalDesktopResolution(int resolutionX, int resolutionY)
   {
      if (Main.mainWindow != null)
      {
         Main.mainWindow.updateInternalDesktopResolution(resolutionX, resolutionY);
      }
   }

   /*
    * Deletes all handlers from the root logger. If not the console logger isn't disabled.
    */
   private static void resetRootLogger()
   {
      Logger rootLogger = Logger.getLogger("");
      Handler[] allRootHandler = rootLogger.getHandlers();

      for(Handler handler : allRootHandler)
      {
         rootLogger.removeHandler(handler);
      }
   }

   private static void initLogger()
   {
      logger = InternalLogger.getLogger(BEEGFS_ADMON_GUI_NAMESPACE);
      logger.log(Level.INFO, "Version: " + BEEGFS_VERSION);

      String guiHost = "localhost";

      try
      {

         guiHost = InetAddress.getLocalHost().getHostName();
      }
      catch (UnknownHostException ex)
      {
         logger.log(Level.WARNING, ex.getMessage());
      }

      logger.log(Level.INFO, "beegfs-admon-GUI on host {0}", guiHost);
   }

   private static boolean initConfigFromFile()
   {
      File configFile;
      String cfgFile = config.getConfiguredCfgFile();
      if (cfgFile == null)
      {
         cfgFile = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_CFG_FILE.getArgsKey());
      }

      if (cfgFile != null)
      {
         // get the config file from command line
         configFile = new File(cfgFile);
         if (!configFile.exists())
         {
            // config file provided by user does not exist
            logger.log(Level.SEVERE, "The configuration file you provided does not exist.");
            JOptionPane.showMessageDialog(null, "The configuration file you provided does not "
                    + "exist.", "Configuration not found", JOptionPane.ERROR_MESSAGE);
            return false;
         }
      }
      else
      {
         // no confguration file specified, try using a default config file
         // try beegfs_dir in users home
         configFile = new File(FilePathsEnum.DEFAULT_CONFIG_FILE.getPath());
      }

      if (!configFile.exists())
      {
         if (!config.hasValidBasicConfig())
         {
            // get configuration from dialog
            JDialogNewConfig newConfig = new JDialogNewConfig(null, true);
            newConfig.setVisible(true);

            if (newConfig.getAbortPushed())
            {
               return false;
            }
         }
      }
      else if (!config.readConfigFile(configFile))
      {
         logger.log(Level.SEVERE, "The configuration file contains errors.");
         JOptionPane.showMessageDialog(null, "The configuration file contains errors.",
                 "Error reading configuration", JOptionPane.ERROR_MESSAGE);
         
         return false;
      }

      logger.setLevel(config.getLogLevel());

      return true;
   }

   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   private static boolean checkJavaVersion()
   {
      String javaVersionString = System.getProperty(PropertyEnum.PROPERTY_JAVA_VERSION.getKey());
      javaVersionString = javaVersionString.substring(0,3);
      double javaVersion = Double.parseDouble(javaVersionString);

      logger.log(Level.INFO, "Used Java version {0}", String.valueOf(javaVersion));

      if (javaVersion < JAVA_MIN_VERSION)
      {
         System.err.println("Java version " + String.valueOf(javaVersion) + " to low. Java version "
                 + JAVA_MIN_VERSION + " is required.");
         logger.log(Level.SEVERE, "Java version {0} to low. Java version {1} is required.",
            new Object[]{String.valueOf(javaVersion), String.valueOf(JAVA_MIN_VERSION)});

         return false;
      }

      return true;
   }

   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   private static boolean checkOsAndPrepare()
   {
      // check if the environment supports a graphical user interface
      if(java.awt.GraphicsEnvironment.isHeadless())
      {
         System.err.println("Your environment doesn't support a graphical user interface. " +
            "Is a X-environment available or X-forwarding in your ssh session enabled?");
         logger.log(Level.SEVERE, "Your environment doesn't support a graphical user interface. " +
            "Is a X-environment available or X-forwarding in your ssh session enabled?");

         return false;
      }

      //disable offscreen pixmap support for X11 Forwording
      System.setProperty(PropertyEnum.PROPERTY_JAVA2D_OFFSCREEN.getKey(), "false");

      // check the local OS to set the L&F correctly
      String osName = System.getProperty(PropertyEnum.PROPERTY_OS_NAME.getKey());
      boolean isMacOsx = osName.startsWith("Mac");
      logger.log(Level.INFO, "Used operating system: {0} ", osName);

      // first tell SkinLF which theme to use
      try
      {
         if(isMacOsx)
         { // set Mac OSX specific configuraions

            // take the menu bar off the jframe
            System.setProperty(PropertyEnum.PROPERTY_APPLE_SCREEN_MENU_BAR.getKey(), "true");

            // set the name of the application menu item
            System.setProperty(PropertyEnum.PROPERTY_APPLE_MENU_ABOUT_NAME.getKey(),
               "BeeGFS admon");

            UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName() );
         }
         else
         { // use BeeGFS look and feel for all other OS
            BeegfsLookAndFeel lf = new BeegfsLookAndFeel();
            UIManager.setLookAndFeel(lf);
         }
      }
      catch (ClassNotFoundException | InstantiationException | IllegalAccessException |
         UnsupportedLookAndFeelException | HeadlessException | NullPointerException ex)
      {
         System.err.println("Please check your graphic environment. " +
            "Is a X-environment available or X-forwarding in your ssh session enabled?");
         logger.log(Level.SEVERE, "Please check your graphic environment. " +
            "Is a X-environment available or X-forwarding in your ssh session enabled? Error:", ex);
         return false;
      }

      return true;
   }
}

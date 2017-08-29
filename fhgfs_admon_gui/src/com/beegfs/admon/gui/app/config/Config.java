package com.beegfs.admon.gui.app.config;

import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.common.enums.PropertyEnum;
import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_ENCODING_UTF8;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.logging.Level;
import javax.naming.ConfigurationException;



import java.util.Locale;
import java.util.ResourceBundle;

public class Config
{
   public static final Level DEFAULT_LOG_LEVEL = Level.INFO;
   private static final String DEFAULT_ADMON_HOST = "localhost";
   private static final String DEFAULT_ADMON_LOCAL = "English";
   private static final int DEFAULT_ADMON_HTTP_PORT = 8000;
   private static final int DEFAULT_RESOLUTION_X = 2048;
   private static final int DEFAULT_RESOLUTION_Y = 2048;
   private static final boolean DEFAULT_LOG_DEBUG = false;
   private static final boolean DEFAULT_IGNORE_VERSION_TEST = false;


   // set default values
   private String admonHost = null;
   private String admonLocal = null;
   private String logFile = null;
   private String cfgFile = null;
   private int admonHttpPort = 0;
   private int resolutionX = 0;
   private int resolutionY = 0;
   private Level logLevel = null;
   private boolean logDebug = false;
   private boolean ignoreVersionTest = false;

   private boolean logDebugIsSet = false;
   private boolean ignoreVersionTestIsSet = false;


   public Config(String args[]) throws ConfigurationException
   {
      initFromArgs(args);
   }

   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   private boolean initFromArgs(String args[]) throws ConfigurationException
   {
      // first check program args
      if (args.length > 0)
      {
         for(String param : args)
         {
            StringTokenizer tokenizer = new StringTokenizer(param, "=");
            if(tokenizer.countTokens() != 2)
            {
               if(tokenizer.countTokens() == 1)
               {
                  String key = tokenizer.nextToken();
                  if (key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_HELP.getArgsKey()))
                  {
                     throw new ConfigurationException(getHelpText());
                  }
               }

               throw new ConfigurationException("Not a valid key value pair: " + param + 
                  System.lineSeparator() + getHelpText());
            }

            String key = tokenizer.nextToken();
            String value = tokenizer.nextToken();

            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_CFG_FILE.getArgsKey()))
            {
               this.cfgFile = value;
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_HOST.getArgsKey()))
            {
               this.admonHost = value;
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_HTTP_PORT.getArgsKey()))
            {
               this.admonHttpPort = Integer.parseInt(value);
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_LOCAL.getArgsKey()))
            {
               this.admonLocal = value;
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_LOG_FILE.getArgsKey()))
            {
               this.logFile = value;
            }
            else
            if(key.equalsIgnoreCase(
               PropertyEnum.PROPERTY_ADMON_GUI_IGNORE_VERSION_TEST.getArgsKey()))
            {
               this.ignoreVersionTest = Boolean.parseBoolean(value);
               this.ignoreVersionTestIsSet = true;
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_LOG_DBG.getArgsKey()))
            {
               this.logDebug = Boolean.parseBoolean(value);
               this.logDebugIsSet = true;
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_LOG_LEVEL.getArgsKey()))
            {
               this.setLogLevel(Integer.parseInt(value));
            }
            else
            if(key.equalsIgnoreCase(PropertyEnum.PROPERTY_ADMON_GUI_RESOLUTION.getArgsKey()))
            {
               this.setResolution(value);
            }
            else
            {
               throw new ConfigurationException("Unknown program option: " + key + "." +
                  System.lineSeparator() + getHelpText());
            }
         }
      }

      // then check Java system properties and set the config if the values wasn't set by args
      String value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_CFG_FILE.getKey());
      if((value != null) && (this.cfgFile == null))
      {
         this.cfgFile = value;
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_HOST.getKey());
      if((value != null) && (this.admonHost == null))
      {
         this.admonHost = value;
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_HTTP_PORT.getKey());
      if((value != null) &&  (this.admonHttpPort == 0))
      {
         this.admonHttpPort = Integer.parseInt(value);
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOG_FILE.getKey());
      if((value != null) &&  (this.logFile == null))
      {
         this.logFile = value;
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_IGNORE_VERSION_TEST.getKey());
      if((value != null) &&  !this.ignoreVersionTestIsSet)
      {
         this.ignoreVersionTest = Boolean.parseBoolean(value);
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOG_DBG.getKey());
      if((value != null) &&  !this.logDebugIsSet)
      {
         this.logDebug = Boolean.parseBoolean(value);
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOG_LEVEL.getKey());
      if((value != null) &&  (this.logLevel == null))
      {
         this.setLogLevel(Integer.parseInt(value));
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOCAL.getKey());
      if((value != null) &&  (this.admonLocal == null))
      {
         this.setAdmonLocal(value);
      }

      value = System.getProperty(PropertyEnum.PROPERTY_ADMON_GUI_RESOLUTION.getKey());
      if((value != null) &&  (this.resolutionX == 0 || this.resolutionY == 0))
      {
         this.setResolution(value);
      }

      return true;
   }

   public String getAdmonHost()
   {
      if (this.admonHost == null)
      {
         return DEFAULT_ADMON_HOST;
      }

      return this.admonHost;
   }

   public int getAdmonHttpPort()
   {
      if (this.admonHttpPort == 0)
      {
         return DEFAULT_ADMON_HTTP_PORT;
      }

      return this.admonHttpPort;
   }

   public String getAdmonLocal()
   {
      if (this.admonLocal == null)
      {
         return DEFAULT_ADMON_LOCAL;
      }

      return this.admonLocal;
   }

   public String getResolution()
   {
      if ((this.resolutionX == 0) && (this.resolutionY == 0))
      {
         return DEFAULT_RESOLUTION_X + "x" + DEFAULT_RESOLUTION_Y;
      }

      return this.resolutionX + "x" + this.resolutionY;
   }

   public int getResolutionX()
   {
      if (this.resolutionX == 0)
      {
         return DEFAULT_RESOLUTION_X;
      }

      return this.resolutionX;
   }

   public int getResolutionY()
   {
      if (this.resolutionY == 0)
      {
         return DEFAULT_RESOLUTION_Y;
      }

      return this.resolutionY;
   }

   public boolean hasValidBasicConfig()
   {
      return (this.admonHost != null) && (this.admonHttpPort != 0);
   }

   public Level getLogLevel()
   {
      if (this.logDebug)
      {
         return Level.ALL;
      }

      if (this.logLevel == null)
      {
         return DEFAULT_LOG_LEVEL;
      }

      return this.logLevel;
   }

   public int getLogLevelNumeric()
   {
      if (this.getLogLevel() == Level.SEVERE)
      {
         return 0;
      }
      else if (this.getLogLevel() == Level.SEVERE)
      {
         return 1;
      }
      else if (this.getLogLevel() == Level.WARNING)
      {
         return 2;
      }
      else if (this.getLogLevel() == Level.INFO)
      {
         return 3;
      }
      else if (this.getLogLevel() == Level.CONFIG)
      {
         return 3;
      }
      else if (this.getLogLevel() == Level.FINE)
      {
         return 4;
      }
      else if (this.getLogLevel() == Level.FINER)
      {
         return 5;
      }
      else if ((this.getLogLevel() == Level.FINEST) || (this.getLogLevel() == Level.ALL))
      {
         return 5;
      }
      return 3;
   }

   public String getLogFile()
   {
      if (this.logFile == null)
      {
         return FilePathsEnum.DEFAULT_LOG_FILE.getPath();
      }

      return this.logFile;
   }

   public String getCfgFile()
   {
      if (this.cfgFile == null)
      {
         return FilePathsEnum.DEFAULT_CONFIG_FILE.getPath();
      }

      return getConfiguredCfgFile();
   }

   public String getConfiguredCfgFile()
   {
      return this.cfgFile;
   }

   public boolean getLogDebug()
   {
      return this.logDebug;
   }

   public boolean getIgnoreVersionTest()
   {
      return this.ignoreVersionTest;
   }

   public void setAdmonHost(String host)
   {
      this.admonHost = host;
   }

   public void setAdmonLocal(String local)
   {
      this.admonLocal = local;
   }
   public void setAdmonHttpPort(int port)
   {
      admonHttpPort = port;
   }

   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   public boolean setResolution(String resolution)
   {
      boolean retVal = false;

      String[] splittedResolution = resolution.split("x");
      if (splittedResolution.length == 2)
      {
         try
         {
            this.resolutionX = Integer.parseInt(splittedResolution[0]);
            this.resolutionY = Integer.parseInt(splittedResolution[1]);

            retVal = true;
         }
         catch (NumberFormatException ex)
         {
            System.err.println("Resolution value isn't valid.");
            retVal = false;
         }
      }

      return retVal;
   }

   public void setResolutionX(int resolutionX)
   {
      this.resolutionX = resolutionX;
   }

   public void setResolutionY(int resolutionY)
   {
      this.resolutionY = resolutionY;
   }

   public void setLogLevel(int logLevel)
   {
      switch (logLevel)
      {
         case 0:
         case 1:
            this.logLevel = Level.SEVERE;
            break;
         case 2:
            this.logLevel = Level.WARNING;
            break;
         case 3:
            this.logLevel = Level.CONFIG;
            break;
         case 4:
            this.logLevel = Level.FINE;
            break;
         case 5:
            this.logLevel = Level.FINEST;
            break;
         default:
      }
   }

   public void setLogLevel(Level logLevel)
   {
      this.logLevel = logLevel;
   }

   public void setLogFile(String logFile)
   {
      this.logFile = logFile;
   }

   public void setCfgFile(String cfgFile)
   {
      this.cfgFile = cfgFile;
   }

   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   public boolean readConfigFile(File f)
   {
      boolean retVal = true;
      FileInputStream fr = null;
      InputStreamReader streamReader = null;

      try
      {
         fr = new FileInputStream(f);
         Properties props = new Properties();
         streamReader = new InputStreamReader(fr, DEFAULT_ENCODING_UTF8);
         props.load(streamReader);

         String newHost = props.getProperty(PropertyEnum.PROPERTY_ADMON_HOST.getConfigKey());
         String newPort = props.getProperty(PropertyEnum.PROPERTY_ADMON_HTTP_PORT.getConfigKey());
         String newResolution = props.getProperty(
            PropertyEnum.PROPERTY_ADMON_GUI_RESOLUTION.getConfigKey());
         String newLocal = props.getProperty(
                 PropertyEnum.PROPERTY_ADMON_GUI_LOCAL.getConfigKey());
         String newLogLevel = props.getProperty(
            PropertyEnum.PROPERTY_ADMON_GUI_LOG_LEVEL.getConfigKey());
         String newLogFile = props.getProperty(
            PropertyEnum.PROPERTY_ADMON_GUI_LOG_FILE.getConfigKey());


         // set the configuration from the configuration file, if the configuration is not set
         if (newHost != null && (this.admonHost == null))
         {
            this.admonHost = newHost;
         }

         if (newPort != null && (this.admonHttpPort == 0))
         {
            this.admonHttpPort = Integer.parseInt(newPort);
         }

         if (newLocal != null && (this.admonLocal == null))
         {
            this.admonLocal = newLocal;
         }

         if (newResolution != null && ((this.resolutionX == 0) || (this.resolutionY == 0)))
         {
            if (!setResolution(newResolution))
            {
               this.resolutionX = DEFAULT_RESOLUTION_X;
               this.resolutionY = DEFAULT_RESOLUTION_Y;
            }
         }

         if ((newLogLevel != null) && (this.logLevel == null))
         {
            setLogLevel(Integer.parseInt(newLogLevel));
         }

         if ((newLogFile != null) && (this.logFile == null))
         {
            this.logFile = newLogFile;
         }
      }
      catch (IOException ex)
      {
         System.err.println("IO error. Could not read configuration file, using defaults." +
            System.lineSeparator() + ex);

         this.admonHost = DEFAULT_ADMON_HOST;
         this.admonHttpPort = DEFAULT_ADMON_HTTP_PORT;
         this.admonLocal = DEFAULT_ADMON_LOCAL;
         this.resolutionX = DEFAULT_RESOLUTION_X;
         this.resolutionY = DEFAULT_RESOLUTION_Y;
         this.logFile = FilePathsEnum.DEFAULT_LOG_FILE.getPath();
         this.cfgFile = FilePathsEnum.DEFAULT_CONFIG_FILE.getPath();
         this.logLevel = DEFAULT_LOG_LEVEL;
         this.logDebug = DEFAULT_LOG_DEBUG;
         this.ignoreVersionTest = DEFAULT_IGNORE_VERSION_TEST;

         retVal = false;
      }
      finally
      {
         if (fr != null)
         {
            try
            {             
               fr.close();
            }
            catch (IOException e)
            {
               System.err.println("Could not close configuration file." + System.lineSeparator()
                  + e);
            }
         }

         if (streamReader != null)
         {
            try
            {
               streamReader.close();
            }
            catch (IOException ex)
            {
               System.err.println("Could not close configuration file." + System.lineSeparator() +
                  ex);
            }
         }
      }
      return retVal;
   }

   @SuppressWarnings("UseOfSystemOutOrSystemErr")
   public boolean writeConfigFile(File f, Properties p)
   {
      boolean retVal = false;
      FileOutputStream fw = null;
      OutputStreamWriter streamWriter = null;

      try
      {
         fw = new FileOutputStream(f);
         streamWriter = new OutputStreamWriter(fw, DEFAULT_ENCODING_UTF8);
         p.store(streamWriter, "");
         retVal = true;
      }
      catch (IOException | ClassCastException ex)
      {
         System.err.println("Could not write configuration file." + ex);
      }
      finally
      {
         if (fw != null)
         {
            try
            {
               fw.close();
            }
            catch (IOException e)
            {
               System.err.println("Could not close configuration file." + e);
            }
         }

         if (streamWriter != null)
         {
            try
            {
               streamWriter.close();
            }
               catch (IOException ex)
               {
                  System.err.println("Could not close configuration file." + ex);
               }
            }
      }
      return retVal;
   }

   public boolean writeConfigFile(File f)
   {
      Properties p = new Properties();
      p.setProperty(PropertyEnum.PROPERTY_ADMON_HOST.getConfigKey(), getAdmonHost());
      p.setProperty(PropertyEnum.PROPERTY_ADMON_HTTP_PORT.getConfigKey(),
         String.valueOf(getAdmonHttpPort()));
      p.setProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOG_FILE.getConfigKey(), getLogFile());
      p.setProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOG_LEVEL.getConfigKey(),
         Integer.toString(getLogLevelNumeric()));

      p.setProperty(PropertyEnum.PROPERTY_ADMON_GUI_LOCAL.getConfigKey(), getAdmonLocal());

      p.setProperty(PropertyEnum.PROPERTY_ADMON_GUI_RESOLUTION.getConfigKey(), getResolution());
      return writeConfigFile(f,p);
   }

   public String getHelpText()
   {
      StringBuilder builder = new StringBuilder(String.format("Usage: java -jar " +
         "beegfs-admon-gui.jar [options]%nValid options: %n"));
      builder.append(String.format("%-50s%s%n", PropertyEnum.PROPERTY_ADMON_GUI_HELP.getArgsKey(),
         "Prints this help text."));
      builder.append(String.format("%-50s%s%n",
         PropertyEnum.PROPERTY_ADMON_GUI_CFG_FILE.getArgsKey() +
         "=$PATH_TO_CONFIG_FILE", "The path to the configuration file."));
      builder.append(String.format("%-50s%s%n", PropertyEnum.PROPERTY_ADMON_HOST.getArgsKey() +
         "=$HOSTNAME_OF_ADMON_SERVER", "The hostname or IP of the admon daemon host."));
      builder.append(String.format("%-50s%s%n", PropertyEnum.PROPERTY_ADMON_HTTP_PORT.getArgsKey() +
         "=$NETWORK_PORT_OF_ADMON_SERVER", "The HTTP network port of the admon daemon."));
      builder.append(String.format("%-50s%s%n",
         PropertyEnum.PROPERTY_ADMON_GUI_LOG_FILE.getArgsKey() +
         "=$PATH_TO_LOG_FILE", "The path to the log file."));
      builder.append(String.format("%-50s%s%n",
         PropertyEnum.PROPERTY_ADMON_GUI_LOG_LEVEL.getArgsKey() +
         "=0|1|2|3|4|5", "The log level. Level 5 prints the most log messages."));

      builder.append(String.format("%-50s%s%n",
              PropertyEnum.PROPERTY_ADMON_GUI_LOCAL.getArgsKey() +
              "=English|Chinese", "The locale is used to set the language environment"));

      builder.append(String.format("%-50s%s%n",
         PropertyEnum.PROPERTY_ADMON_GUI_RESOLUTION.getArgsKey() +
         "=$RESOLUTION_Xx$RESOLUTION_Y", "The resolution in pixels for the X- and Y- axis of the"));
      builder.append(String.format("%-50s%s%n%n", "", "admon gui main window desktop frame."));
      builder.append("Example: java -jar beegfs-admon-gui.jar --admonHost=beegfs01 " +
         "--admonHttpPort=8112 --resolution=1024x1024%n%n");

      return builder.toString();
   }
}

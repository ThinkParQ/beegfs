package com.beegfs.admon.gui.app.log;

import com.beegfs.admon.gui.program.Main;
import java.io.IOException;
import java.util.logging.Handler;
import java.util.logging.LogManager;
import java.util.logging.Logger;


public class InternalLogger extends Logger
{
   private static Handler logGuiHandler;

   public static InternalLogger getLogger(String name)
   {
      LogManager manager = LogManager.getLogManager();
      Object logger = manager.getLogger(name);

      if (logger == null)
      {
         manager.addLogger(new InternalLogger(name));
      }

      logger = manager.getLogger(name);
      return (InternalLogger)logger;
   }

   protected InternalLogger(String name)
   {
      super(name, null);
      init();
   }

   // Ignore e.printStackTrace() warning, because if logger couln't start a stack trace must be
   // printed to the console
   @SuppressWarnings({"CallToThreadDumpStack", "CallToPrintStackTrace", "UseOfSystemOutOrSystemErr"})
   private void init()
   {
      try
      {
         // This block configures the logger with handler and formatter
         setLevel(Main.getConfig().getLogLevel());

         String newLogFile = Main.getConfig().getLogFile();
         logGuiHandler = new LogHandlerGui(newLogFile);
         addHandler(logGuiHandler);

         setUseParentHandlers(true);
      }
      catch (IOException e)
      {
         // logging only to system.err possible
         System.err.println(e.getMessage() + System.lineSeparator());
         e.printStackTrace();
      }
   }
}

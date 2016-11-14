package com.beegfs.admon.gui.app.log;

import com.beegfs.admon.gui.components.panels.StatusPanel;
import com.beegfs.admon.gui.program.Main;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TreeMap;
import java.util.logging.FileHandler;
import java.util.logging.Formatter;
import java.util.logging.Level;
import java.util.logging.LogRecord;
import java.util.logging.Logger;
import javax.swing.JTextPane;


public class LogHandlerGui extends FileHandler
{
   static final Logger LOGGER = Logger.getLogger(LogHandlerGui.class.getCanonicalName());

   private static final int INTERNAL_LOGGER_MAX_LOG_FILE_SIZE = 20971520; // 20MiB
   private static final int INTERNAL_LOGGER_MAX_LOG_FILE_COUNT = 5;

   private final TreeMap<String, Long> loggedData;

   private final Formatter formatter;

   public LogHandlerGui(String logFilePath) throws IOException
   {
      super(logFilePath, INTERNAL_LOGGER_MAX_LOG_FILE_SIZE, INTERNAL_LOGGER_MAX_LOG_FILE_COUNT,
         false);

      formatter = new LogFormatter(this.getLevel());
      setFormatter(formatter);

      loggedData = new TreeMap<>();
   }

   @Override
   public void publish(LogRecord record)
   {
      if (shouldLog(record.getMessage()))
      {
         // log into log file
         super.publish(record);

         if(record.getParameters() != null)
         {
            try
            {
               // log into gui components
               Object[] parameters = record.getParameters();
               for (Object parameter : parameters)
               {
                  if (parameter instanceof Boolean)
                  {
                     if (((Boolean) parameter))
                     {
                        logToDisplay(record);
                     }
                  }
                  else if (parameter instanceof JTextPane)
                  {
                     logToTextPane(record, (JTextPane) parameter);
                  }
                  else if (parameter instanceof StatusPanel)
                  {
                     logToStatusPanel(record, (StatusPanel) parameter);
                  }
               }
            }
            catch (NullPointerException npe)
            {
               LOGGER.log(Level.SEVERE, npe.getMessage(), npe);
            }
         }
      }
   }

   @Override
   public void flush()
   {
      super.flush();
   }

   @Override
   public void close() throws SecurityException
   {
      super.close();
   }

   private synchronized Long getLoggedData(String msg)
   {
      return loggedData.get(msg);
   }

   private synchronized Long removeLoggedData(String msg)
   {
      return loggedData.remove(msg);
   }

   private synchronized Long putLoggedData(String msg, Long time)
   {
      return loggedData.put(msg, time);
   }

   private boolean shouldLog(String msg)
   {
      // only write an event every 5 minutes to logger (otherwise log gets too full)
      Date dt = new Date();
      Long now = dt.getTime();
      Long time = getLoggedData(msg);
      
      if (time != null)
      {
         if ((now - time) < (5 * 60 * 1000))
         {
            return false;
         }
         else
         {
            removeLoggedData(msg);
            return true;
         }
      }
      return true;
   }

   public void logToDisplay(LogRecord record)
   {
      Date dt = new Date(record.getMillis());
      String text = getDateString() + record.getMessage();

      putLoggedData(record.getMessage(), dt.getTime());

      JTextPane textPane = Main.getMainWindow().getStatusPanel().getTextPane();
      if (!textPane.getText().contains(record.getMessage()))
      {
         textPane.setText(textPane.getText() + text + System.lineSeparator());
         textPane.revalidate();
      }
   }

   public void logToTextPane(LogRecord record, JTextPane textPane)
   {
      String text = getDateString() + record.getMessage();

      if (!textPane.getText().contains(record.getMessage()))
      {
         textPane.setText(textPane.getText() + text + System.lineSeparator());
         textPane.revalidate();
      }
   }

   public void logToStatusPanel(LogRecord record, StatusPanel panel)
   {
      String text = getDateString() + record.getMessage();
      panel.addLine(text, true);
   }

   private String getDateString()
   {
      Date dt = new Date();
      SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
      return "[" + df.format(dt) + "] ";
   }
}

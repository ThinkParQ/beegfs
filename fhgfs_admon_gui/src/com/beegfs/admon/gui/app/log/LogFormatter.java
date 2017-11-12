package com.beegfs.admon.gui.app.log;

import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.Writer;
import java.text.MessageFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.logging.Formatter;
import java.util.logging.Level;
import java.util.logging.LogRecord;


public class LogFormatter extends Formatter
{

   public static String getStackTrace(Throwable aThrowable)
   {
      final Writer result = new StringWriter();
      final PrintWriter printWriter = new PrintWriter(result);
      aThrowable.printStackTrace(printWriter);
      return result.toString();
   }
   private final Level sysLogLevel;


   public LogFormatter(Level logLevel)
   {
      super();
      sysLogLevel = logLevel;
   }


   @Override
   public String format(LogRecord rec)
   {
      StringBuilder sb = new StringBuilder(DEFAULT_STRING_LENGTH);
      sb.append(getLogLevel(rec.getLevel()));
      sb.append(calcDate(rec.getMillis()));
      sb.append(" [");
      sb.append(rec.getSourceClassName());
      sb.append(".");
      sb.append(rec.getSourceMethodName());
      sb.append("] >> ");
      sb.append(MessageFormat.format(rec.getMessage(),rec.getParameters()));

      if (sysLogLevel == Level.ALL)
      { // if debug logging is active, log with stack trace
         Throwable thrown = rec.getThrown();
         if (thrown != null)
         {
            sb.append(System.lineSeparator());
            sb.append(getStackTrace(thrown));
         }
      }

      sb.append(System.lineSeparator());

      return sb.toString();
   }

   private String calcDate(long millisecs)
   {
      SimpleDateFormat dateFormat = new SimpleDateFormat("MMMdd HH:mm:ss");
      Date resultdate = new Date(millisecs);
      return dateFormat.format(resultdate);
   }

   private String getLogLevel(Level logLevel)
   {
      String level = "(";

      if (logLevel == Level.SEVERE)
      {
         level += "0";
      }
      else if (logLevel == Level.WARNING)
      {
         level += "2";
      }
      else if (logLevel == Level.INFO)
      {
         level += "3";
      }
      else if (logLevel == Level.CONFIG)
      {
         level += "3";
      }
      else if (logLevel == Level.FINE)
      {
         level += "4";
      }
      else if (logLevel == Level.FINER)
      {
         level += "5";
      }
      else if (logLevel == Level.FINEST)
      {
         level += "5";
      }

      level += ") ";

      return level;
   }

}

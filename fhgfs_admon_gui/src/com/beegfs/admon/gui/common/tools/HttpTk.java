package com.beegfs.admon.gui.common.tools;

import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_ENCODING_UTF8;
import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import com.beegfs.admon.gui.program.Main;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStreamWriter;
import java.net.URL;
import java.net.URLConnection;
import java.util.logging.Level;
import java.util.logging.Logger;

public class HttpTk
{
   static final Logger LOGGER = Logger.getLogger(HttpTk.class.getCanonicalName());

   public static InputStream sendPostData(String urlStr, Object[] parameters)
   {
      OutputStreamWriter wr = null;
      
      try
      {
         StringBuilder data = new StringBuilder(DEFAULT_STRING_LENGTH);
         for (int i = 0; i < parameters.length - 1; i += 2)
         {
            String key = parameters[i].toString();
            String value = parameters[i + 1].toString();
            data.append(key);
            data.append("=");
            data.append(value);
            data.append("&");
         }

         // Send data
         URL url = new URL(urlStr);
         URLConnection conn = url.openConnection();
         conn.setDoOutput(true);
         wr = new OutputStreamWriter(conn.getOutputStream(),
            DEFAULT_ENCODING_UTF8);
         wr.write(data.toString());
         wr.flush();
         wr.close();

         // Get the response
         return conn.getInputStream();
      }
      catch (IOException e)
      {
         LOGGER.log(Level.SEVERE, "Error during sending POST data to URL: " + urlStr, e);
         return null;
      }
      finally
      {
         if (wr != null)
         {
            try
            {
               wr.close();
            }
            catch (IOException ex)
            {
               LOGGER.log(Level.SEVERE, "Error during sending POST data to URL: " + urlStr, ex);
            }
         }
      }
   }

   public static String generateAdmonUrl(String urlPath)
   {
      return "http://" + Main.getConfig().getAdmonHost() + ":"
              + Main.getConfig().getAdmonHttpPort() + urlPath;
   }

   private HttpTk()
   {
   }
}

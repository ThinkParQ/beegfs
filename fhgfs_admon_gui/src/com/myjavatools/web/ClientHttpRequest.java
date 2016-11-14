package com.myjavatools.web;


import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;
import java.util.Observable;
import java.util.Random;
import java.util.concurrent.atomic.AtomicBoolean;


/**
 * <p>
 * Title: MyJavaTools: Client HTTP Request class</p>
 * <p>
 * Description: this class helps to send POST HTTP requests with various form data, including files.
 * Cookies can be added to be included in the request.</p>
 *
 * Licensed under the myjavatools license (Apache license version 2.0)
 * http://www.myjavatools.com/license.txt
 *
 * Fix some findbugs issues and convert to BeeGFS code style; by BeeGFS Team.
 * Replace setParameter(String name, String value); by BeeGFS Team
 * Modify doPost(); by BeeGFS Team
 *
 * @author Vlad Patryshev
 * @author James Peltzer
 * @author BeeGFS team
 * @version 6.0
 */
public class ClientHttpRequest extends Observable
{
   private static final int DEFAULT_COOKIE_CAPACITY = 5;
   public static final String DEFAULT_ENCODING_UTF8 = "UTF-8";
   
   private static Random random = new Random();

   protected static String randomString()
   {
      return Long.toString(random.nextLong(), 36);
   }

   /**
    * Posts a new request to specified URL, with parameters that are passed in the argument
    *
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.util.Map)
    */
   public static InputStream post(URL url, Map<String, String> parameters) throws IOException
   {
      return new ClientHttpRequest(url).post(parameters);
   }

   /**
    * Posts a new request to specified URL, with parameters that are passed in the argument
    *
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.lang.Object...)
    */
   public static InputStream post(URL url, Object[] parameters) throws IOException
   {
      return new ClientHttpRequest(url).post(parameters);
   }

   /**
    * Posts a new request to specified URL, with cookies and parameters that are passed in the
    * argument
    *
    * @param cookies request cookies
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setCookies(java.util.Map) 
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.util.Map)
    */
   public static InputStream post(URL url, Map<String, String> cookies,
      Map<String, String> parameters) throws IOException
   {
      return new ClientHttpRequest(url).post(cookies, parameters);
   }

   /**
    * Posts a new request to specified URL, with cookies and parameters that are passed in the
    * argument
    *
    * @param cookies request cookies
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setCookies(java.lang.String[])
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.lang.Object...)
    */
   public static InputStream post(URL url, String[] cookies, Object[] parameters) throws
      IOException
   {
      return new ClientHttpRequest(url).post(cookies, parameters);
   }

   private URLConnection connection;
   private OutputStream os = null;
   private final Map<String, String> cookies = new HashMap<>(DEFAULT_COOKIE_CAPACITY);
   private String rawCookies = "";

   private final String boundary = "---------------------------" +
      randomString() + randomString() + randomString();
   private final AtomicBoolean isCanceled = new AtomicBoolean(false);
   private int bytesSent = 0;

   /**
    * Creates a new multipart POST HTTP request on a freshly opened URLConnection
    *
    * @param connection an already open URL connection
    * @throws IOException
    */
   public ClientHttpRequest(URLConnection connection) throws IOException
   {
      this.connection = connection;
      connection.setDoOutput(true);
      connection.setDoInput(true);
      connection.setRequestProperty("Content-Type",
         "multipart/form-data; boundary=" + boundary);
   }

   /**
    * Creates a new multipart POST HTTP request for a specified URL
    *
    * @param url the URL to send request to
    * @throws IOException
    */
   public ClientHttpRequest(URL url) throws IOException
   {
      this(url.openConnection());
   }

   /**
    * Creates a new multipart POST HTTP request for a specified URL string
    *
    * @param urlString the string representation of the URL to send request to
    * @throws IOException
    */
   public ClientHttpRequest(String urlString) throws IOException
   {
      this(new URL(urlString));
   }

   protected void connect() throws IOException
   {
      if (os == null)
      {
         os = connection.getOutputStream();
      }
   }

   protected void write(char c) throws IOException
   {
      connect();
      os.write(c);
   }

   protected void write(String s) throws IOException
   {
      connect();
      os.write(s.getBytes(DEFAULT_ENCODING_UTF8));
   }

   protected long newlineNumBytes()
   {
      return 2;
   }

   protected void newline() throws IOException
   {
      connect();
      write("\r\n"); // do not replace with System.lineSeparator()
   }

   protected void writeln(String s) throws IOException
   {
      connect();
      write(s);
      newline();
   }

   private long boundaryNumBytes()
   {
      return boundary.length() + 2 /*--*/;
   }

   private void boundary() throws IOException
   {
      write("--");
      write(boundary);
   }

   private void postCookies()
   {
      StringBuilder cookieList = new StringBuilder(rawCookies);
      for (Map.Entry<String, String> cookie : cookies.entrySet())
      {
         if (cookieList.length() > 0)
         {
            cookieList.append("; ");
         }
         cookieList.append(cookie.getKey());
         cookieList.append("=");
         cookieList.append(cookie.getValue());
      }
      if (cookieList.length() > 0)
      {
         connection.setRequestProperty("Cookie", cookieList.toString());
      }
   }

   /**
    * Adds a cookie to the requst
    *
    * @param rawCookies request cookies
    * @throws IOException
    */
   public void setCookies(String rawCookies) throws IOException
   {
      this.rawCookies = (rawCookies == null) ? "" : rawCookies;
      cookies.clear();
   }

   /**
    * Adds a cookie to the requst
    *
    * @param name cookie name
    * @param value cookie value
    * @throws IOException
    */
   public void setCookie(String name, String value)
      throws IOException
   {
      cookies.put(name, value);
   }

   /**
    * Adds cookies to the request
    *
    * @param cookies the cookie "name-to-value" map
    * @throws IOException
    */
   public void setCookies(Map<String, String> cookies) throws IOException
   {
      if (cookies != null)
      {
         this.cookies.putAll(cookies);
      }
   }

   /**
    * Adds cookies to the request
    *
    * @param cookies array of cookie names and values (cookies[2*i] is a name, cookies[2*i + 1] is a
    * value)
    * @throws IOException
    */
   public void setCookies(String[] cookies) throws IOException
   {
      if (cookies != null)
      {
         for (int i = 0; i < cookies.length - 1; i += 2)
         {
            setCookie(cookies[i], cookies[i + 1]);
         }
      }
   }

   private long writeNameNumBytes(String name) throws UnsupportedEncodingException
   {
      return newlineNumBytes() +
         "Content-Disposition: form-data; name=\"".length() +
         name.getBytes(DEFAULT_ENCODING_UTF8).length +
         1 /*'"'*/;
   }

   private void writeName(String name) throws IOException
   {
      newline();
      write("Content-Disposition: form-data; name=\"");
      write(name);
      write('"');
   }

   public synchronized int getBytesSent()
   {
      return bytesSent;
   }

   public void cancel()
   {
      isCanceled.set(true);
   }

   // suppress some warnings to keep original code
   @SuppressWarnings({"NestedSynchronizedStatement", "UnusedAssignment"})
   private synchronized void pipe(InputStream in, OutputStream out) throws IOException
   {
      byte[] buf = new byte[1024];
      int nread;
      bytesSent = 0;
      isCanceled.set(false);
      synchronized (in)
      {
         nread = in.read(buf, 0, buf.length);
         while (nread >= 0)
         {
            out.write(buf, 0, nread);
            bytesSent += nread;
            if (isCanceled.get())
            {
               throw new IOException("Canceled");
            }
            out.flush();
            this.setChanged();
            this.notifyObservers(bytesSent);
            this.clearChanged();

            nread = in.read(buf, 0, buf.length);
         }
      }
      out.flush();
      buf = null;
   }

   /* replace setParameter(String name, String value) with a BeeGFS version; by BeeGFS Team
   public void setParameter(String name, String value) throws IOException
   {
      boundary();
      writeName(name);
      newline();
      newline();
      writeln(value);
   }
   */
   /**
    * Adds a string parameter to the request
    *
    * @param name parameter name
    * @param value parameter value
    * @throws IOException
    */
   public void setParameter(String name, String value) throws IOException
   {
      String outStr = name + "=" + value + "&";
      write(outStr);
   }

   /**
    * Adds a file parameter to the request
    *
    * @param name parameter name
    * @param filename the name of the file
    * @param is input stream to read the contents of the file from
    * @throws IOException
    */
   public void setParameter(String name, String filename, InputStream is) throws IOException
   {
      boundary();
      writeName(name);
      write("; filename=\"");
      write(filename);
      write('"');
      newline();
      write("Content-Type: ");
      String type = URLConnection.guessContentTypeFromName(filename);
      if (type == null)
      {
         type = "application/octet-stream";
      }
      writeln(type);
      newline();
      pipe(is, os);
      newline();
   }

   public long getFilePostSize(String name, File file) throws UnsupportedEncodingException
   {
      String filename = file.getPath();
      String type = URLConnection.guessContentTypeFromName(filename);
      if (type == null)
      {
         type = "application/octet-stream";
      }

      return boundaryNumBytes() +
         writeNameNumBytes(name) +
         "; filename=\"".length() +
         filename.getBytes(DEFAULT_ENCODING_UTF8).length +
         1 +
         newlineNumBytes() +
         "Content-Type: ".length() +
         type.length() +
         newlineNumBytes() +
         newlineNumBytes() +
         file.length() +
         newlineNumBytes();
   }

   /**
    * Adds a file parameter to the request
    *
    * @param name parameter name
    * @param file the file to upload
    * @throws IOException
    */
   public void setParameter(String name, File file) throws IOException
   {
      try (FileInputStream fis = new FileInputStream(file))
      {
         setParameter(name, file.getPath(), fis);
      }
   }

   /**
    * Adds a parameter to the request; if the parameter is a File, the file is uploaded, otherwise
    * the string value of the parameter is passed in the request
    *
    * @param name parameter name
    * @param object parameter value, a File or anything else that can be stringified
    * @throws IOException
    */
   public void setParameter(Object name, Object object) throws IOException
   {
      if (object instanceof File)
      {
         setParameter(name.toString(), (File) object);
      }
      else
      {
         setParameter(name.toString(), object.toString());
      }
   }

   /**
    * Adds parameters to the request
    *
    * @param parameters "name-to-value" map of parameters; if a value is a file, the file is
    * uploaded, otherwise it is stringified and sent in the request
    * @throws IOException
    */
   public void setParameters(Map<String, String> parameters) throws IOException
   {
      if (parameters != null)
      {
         for (Map.Entry<String, String> entry : parameters.entrySet())
         {
            setParameter(entry.getKey(), entry.getValue());
         }
      }
   }

   /**
    * Adds parameters to the request
    *
    * @param parameters (vararg) parameter names and values (parameters[2*i] is a name,
    * parameters[2*i + 1] is a value); if a value is a file, the file is uploaded, otherwise it is
    * stringified and sent in the request
    * @throws IOException
    */
   public void setParameters(Object... parameters) throws IOException
   {
      for (int i = 0; i < parameters.length - 1; i += 2)
      {
         setParameter(parameters[i].toString(), parameters[i + 1]);
      }
   }

   public long getPostFooterSize()
   {
      return boundaryNumBytes() + 2 /*--*/ +
         newlineNumBytes() + newlineNumBytes();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added
    *
    * @return input stream with the server response
    * @throws IOException
    */
   private InputStream doPost() throws IOException
   {
      /* delete unneeded functions; by BeeGFS Team
      boundary();
      writeln("--");
      */
      os.close();

      return connection.getInputStream();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added
    *
    * @return input stream with the server response
    * @throws IOException
    */
   public InputStream post() throws IOException
   {
      postCookies();
      return doPost();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added before
    * (if any), and with parameters that are passed in the argument
    *
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.util.Map)
    */
   public InputStream post(Map<String, String> parameters) throws IOException
   {
      postCookies();
      setParameters(parameters);
      return doPost();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added before
    * (if any), and with parameters that are passed in the argument
    *
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.lang.Object...)
    */
   public InputStream post(Object... parameters) throws IOException
   {
      postCookies();
      setParameters(parameters);
      return doPost();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added before
    * (if any), and with cookies and parameters that are passed in the arguments
    *
    * @param cookies request cookies
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.util.Map)
    * @see com.myjavatools.web.ClientHttpRequest#setCookies(java.util.Map) 
    */
   public InputStream post(Map<String, String> cookies, Map<String, String> parameters) throws
      IOException
   {
      setCookies(cookies);
      postCookies();
      setParameters(parameters);
      return doPost();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added before
    * (if any), and with cookies and parameters that are passed in the arguments
    *
    * @param raw_cookies request cookies
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.util.Map)
    * @see com.myjavatools.web.ClientHttpRequest#setCookies(java.lang.String) 
    */
   public InputStream post(String raw_cookies, Map<String, String> parameters) throws IOException
   {
      setCookies(raw_cookies);
      postCookies();
      setParameters(parameters);
      return doPost();
   }

   /**
    * Posts the requests to the server, with all the cookies and parameters that were added before
    * (if any), and with cookies and parameters that are passed in the arguments
    *
    * @param cookies request cookies
    * @param parameters request parameters
    * @return input stream with the server response
    * @throws IOException
    * @see com.myjavatools.web.ClientHttpRequest#setParameters(java.lang.Object...)
    * @see com.myjavatools.web.ClientHttpRequest#setCookies(java.lang.String[])
    */
   public InputStream post(String[] cookies, Object[] parameters) throws IOException
   {
      setCookies(cookies);
      postCookies();
      setParameters(parameters);
      return doPost();
   }

}

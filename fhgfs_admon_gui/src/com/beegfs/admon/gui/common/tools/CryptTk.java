package com.beegfs.admon.gui.common.tools;

import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import java.util.logging.Level;
import java.util.logging.Logger;


public class CryptTk
{
   static final Logger LOGGER = Logger.getLogger(CryptTk.class.getCanonicalName());

   /**
    * char[] to byte[] conversion
    *
    * @param chars - char[] to convert
    * @return byte[]
    */
   public static byte[] charsToBytes(char[] chars)
   {
      byte[] bytes = new byte[chars.length];
      int i;
      for (i = 0; i < chars.length; i++)
      {
         bytes[i] = (byte) (chars[i] & 0xFF);
      }
      return bytes;
   }

   /**
    * byte[] to String conversion
    *
    * @param bytes - byte[] to convert
    * @return String
    */
   public static String bytesToString(byte[] bytes)
   {
      StringBuilder sb = new StringBuilder(DEFAULT_STRING_LENGTH);
      String s;
      int i;

      for (i = 0; i < bytes.length; i++)
      {
         if (i % 32 == 0 && i != 0)
         {
            sb.append(System.lineSeparator());
         }
         s = Integer.toHexString(bytes[i]);
         if (s.length() < 2)
         {
            s = "0" + s;
         }
         if (s.length() > 2)
         {
            s = s.substring(s.length() - 2);
         }
         sb.append(s);
      }
      return sb.toString();
   }

   /**
    * Create MD5 String
    *
    * @param chars - password as char[]
    * @return String - an MD5 hash of the field text
    */
   public static String getMD5(char[] chars)
   {
      java.security.MessageDigest md;
      String MD5;
      try
      {
         md = java.security.MessageDigest.getInstance("MD5");
      }
      catch (java.security.NoSuchAlgorithmException e)
      {
         LOGGER.log(Level.FINEST, "Algorithm error.", e);
         return null;
      }
      MD5 = bytesToString(md.digest(charsToBytes(chars)));
      return MD5;
   }

   public static String cryptWithNonce(String cryptedStr, long nonce)
   {
      String str = cryptedStr + String.valueOf(nonce);
      String crypted = getMD5(str.toCharArray());
      return crypted;
   }

   private CryptTk()
   {
   }
}

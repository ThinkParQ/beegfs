package com.beegfs.admon.gui.common.exceptions;

/**
 * Should be used if the communication between the admon GUI and the admon daemon fails.
 */
public class CommunicationException extends Exception
{

   /**
    * Creates a new instance of <code>CommunicationException</code> without detail message.
    */
   public CommunicationException()
   {
   }


   /**
    * Constructs an instance of <code>CommunicationException</code> with the specified detail
    * message.
    * @param msg the detail message.
    */
   public CommunicationException(String msg)
   {
      super(msg);
   }
}

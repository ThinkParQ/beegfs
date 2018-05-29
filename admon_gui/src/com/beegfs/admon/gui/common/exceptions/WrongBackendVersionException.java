package com.beegfs.admon.gui.common.exceptions;


/**
 * Should be used if the version of the admon GUI and the admon daemon is different.
 */
public class WrongBackendVersionException extends Exception
{

   /**
    * Creates a new instance of <code>CommunicationException</code> without detail message.
    */
   public WrongBackendVersionException()
   {
   }


   /**
    * Constructs an instance of <code>CommunicationException</code> with the specified detail
    * message.
    * @param msg the detail message.
    */
   public WrongBackendVersionException(String msg)
   {
      super(msg);
   }
}

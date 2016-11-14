package com.beegfs.admon.gui.common.threading;

import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JProgressBar;

public class ProgressBarThread extends GuiThread
{
   static final Logger LOGGER = Logger.getLogger(ProgressBarThread.class.getCanonicalName());

   private JProgressBar bar;

   public ProgressBarThread(JProgressBar bar, String text, String name)
   {
      this(bar, name);
      this.bar.setString(text);
      this.bar.setStringPainted(true);
   }

   public ProgressBarThread(JProgressBar bar, String name)
   {
      super(name);
      this.bar = bar;
      this.bar.setStringPainted(false);
   }

   @Override
   public void run()
   {
      bar.setVisible(true);
      int value = 0;
      while (!stop)
      {
         value++;
         bar.setValue(value);
         try
         {
            sleep(50);
         }
         catch (InterruptedException e)
         {
            LOGGER.log(Level.FINEST, "Internal error.", e);
         }
         if (value >= 100)
         {
            value = 0;
         }
      }
      bar.setVisible(false);
   }
}
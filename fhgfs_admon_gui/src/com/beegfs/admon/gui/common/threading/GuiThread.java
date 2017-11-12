package com.beegfs.admon.gui.common.threading;


public class GuiThread extends Thread
{
   protected static final int DEFAULT_LOOP_SLEEP_MS = 8000;

   @SuppressWarnings("ProtectedField")
   protected volatile boolean stop;

   public GuiThread(String name)
   {
      super(name);
      stop = false;
   }

   public GuiThread(Runnable r)
   {
      super(r);
      stop = false;
   }

   public void shouldStop()
   {
      stop = true;
   }

   public boolean isStopping()
   {
      return stop;
   }
}

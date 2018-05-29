package com.beegfs.admon.gui.common.threading;

import com.beegfs.admon.gui.common.XMLParser;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * A thread for updating the data of a GUI. Only the run method must be implemented.
 */
public class UpdateThread extends GuiThread
{
   protected final XMLParser parser;
   protected final Lock lock;
   protected final Condition gotData;

   public UpdateThread(String parserUrl, String name)
   {
      this(parserUrl, false, DEFAULT_LOOP_SLEEP_MS, name);
   }

   public UpdateThread(String parserUrl, boolean oneshot, String name)
   {
      this(parserUrl, oneshot, DEFAULT_LOOP_SLEEP_MS, name);
   }

   public UpdateThread(String parserUrl, boolean oneshot, int loopSleepMS, String name)
   {
      super(name);
      lock = new ReentrantLock();
      parser = new XMLParser(parserUrl, oneshot, lock, loopSleepMS, name);
      gotData = parser.getDataCondition();
   }

   public Condition getDataCondition()
   {
      return gotData;
   }

   public XMLParser getParser()
   {
      return parser;
   }

   @Override
   public void start()
   {
      super.start();
      parser.start();
   }

   public void startWithoutParser()
   {
      super.start();
   }

   void updateUrl(String newUrl)
   {
      parser.setUrl(newUrl);
   }

   @Override
   public void shouldStop()
   {
      super.shouldStop();
      parser.shouldStop();
   }
}

package com.beegfs.admon.gui.components.listeners;

import java.awt.event.AdjustmentEvent;
import java.awt.event.AdjustmentListener;
import javax.swing.JDesktopPane;
import javax.swing.JInternalFrame;

public class RepaintAdjustmentListener implements AdjustmentListener
{

   private final JDesktopPane desktop;

   public RepaintAdjustmentListener(JDesktopPane desktop)
   {
      super();
      this.desktop = desktop;
   }


   @Override
   public void adjustmentValueChanged(AdjustmentEvent e)
   {
      //return if the user is changing the scrollbars
      if (e.getValueIsAdjusting())
      {
         return;
      }

      //refresh all JInternalFrames if the values of the scrollbars wars changed
      JInternalFrame[] frames = desktop.getAllFrames();
      for (JInternalFrame f : frames)
      {
         f.repaint();
      }
   }
}

package com.beegfs.admon.gui.components.managers;

import javax.swing.DefaultDesktopManager;
import javax.swing.JDesktopPane;
import javax.swing.JInternalFrame;


public class DesktopManager extends DefaultDesktopManager
{
   private static final long serialVersionUID = 1L;

   private final JDesktopPane desktop;

   public DesktopManager(JDesktopPane p)
   {
      super();
      desktop = p;
   }

   @Override
   public void activateFrame(JInternalFrame f)
   {
      super.activateFrame(f);
   }
}

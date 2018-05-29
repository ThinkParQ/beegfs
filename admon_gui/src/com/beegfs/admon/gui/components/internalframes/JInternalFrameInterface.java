package com.beegfs.admon.gui.components.internalframes;


import java.awt.Rectangle;
import java.beans.PropertyVetoException;



/**
 * Interface for all InternalFrame classes
 *
 */
public interface JInternalFrameInterface
{
   boolean isEqual(JInternalFrameInterface obj);
   String getFrameTitle();
   public void setIcon(boolean b) throws PropertyVetoException;
   public void setSelected(boolean selected) throws PropertyVetoException;
   public void toFront();
   public Rectangle getBounds(Rectangle rv);
}

package com.beegfs.admon.gui.components.internalframes;


import com.beegfs.admon.gui.common.enums.UpdateDataTypeEnum;
import java.util.ArrayList;



/**
 * Interface for all JInternalFrame classes which opens a JFrame or JDialog which can update
 * data of this JInternalFrame
 */
public interface JInternalFrameUpdateableInterface extends JInternalFrameInterface
{
   public void updateData(ArrayList<String> data, UpdateDataTypeEnum type);
}

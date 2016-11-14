package com.beegfs.admon.gui.components.tables;

import java.awt.Component;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.DefaultTableCellRenderer;

public class FileBrowserTableCellRenderer extends DefaultTableCellRenderer
{

   public FileBrowserTableCellRenderer()
   {
      super();
   }

   @Override
   public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected,
           boolean hasFocus, int row, int column)
   {
      String valueStr = "";
      
      if (value != null)
      {
         valueStr = value.toString();
      }

      setText(valueStr);
      this.setHorizontalAlignment(JLabel.CENTER);
      this.setVisible(true);
      this.setToolTipText(valueStr);
      return this;
   }
}

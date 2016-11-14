package com.beegfs.admon.gui.components.tables;

import java.awt.Color;
import java.awt.Component;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.DefaultTableCellRenderer;

public class FileBrowserTableCellRendererFilename extends DefaultTableCellRenderer
{
   private static final long serialVersionUID = 1L;

   public FileBrowserTableCellRendererFilename()
   {
      super();
   }

   @Override
   public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected,
           boolean hasFocus, int row, int column)
   {
      FilenameObject obj = (FilenameObject) value;
      setText(obj.getName());
      
      if (obj.isDirectory())
      {
         this.setForeground(Color.BLUE);
      }
      else
      {
         this.setForeground(Color.BLACK);
      }
      this.setHorizontalAlignment(JLabel.LEADING);
      this.setVisible(true);
      return this;
   }
}
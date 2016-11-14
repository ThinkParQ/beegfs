package com.beegfs.admon.gui.components.tables;

import java.awt.Color;
import java.awt.Component;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.TableCellRenderer;

public class FileBrowserTableCellRendererStriping extends JButton implements TableCellRenderer
{
   private static final long serialVersionUID = 1L;

   public FileBrowserTableCellRendererStriping()
   {
      super();
   }

   @Override
   public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected,
           boolean hasFocus, int row, int column)
   {
      FilenameObject obj = (FilenameObject) value;
      if (obj.isDirectory())
      {
         setText("Settings");
         this.setForeground(Color.BLUE);
         this.setHorizontalAlignment(JLabel.CENTER);
      }
      else
      {
         setText("Display");
         this.setForeground(Color.BLUE);
         this.setHorizontalAlignment(JLabel.CENTER);
      }
      return this;
   }
}

package com.beegfs.admon.gui.components.tables;

import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import java.awt.Component;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.DefaultTableCellRenderer;


public class IBConfigTableHeaderRenderer extends DefaultTableCellRenderer
{
   private static final long serialVersionUID = 1L;

   private final String tooltip;

   public IBConfigTableHeaderRenderer(String tooltip)
   {
      super();
      this.tooltip = tooltip;
   }

   @Override
   public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected,
           boolean hasFocus, int row, int column)
   {
      try
      {
         setText(value.toString());
      }
      catch (java.lang.NullPointerException e)
      {
         setText("");
      }

      this.setHorizontalAlignment(JLabel.CENTER);
      if (!tooltip.isEmpty())
      {
         this.setToolTipText(tooltip);
         this.setHorizontalTextPosition(JLabel.LEADING);
         this.setIcon(new javax.swing.ImageIcon(IBConfigTableHeaderRenderer.class.getResource(
                 FilePathsEnum.IMAGE_INFO.getPath()))); // NOI18N
      }
      this.setVisible(true);
      return this;
   }
}

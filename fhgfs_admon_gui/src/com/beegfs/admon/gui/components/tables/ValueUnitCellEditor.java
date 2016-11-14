package com.beegfs.admon.gui.components.tables;


import com.beegfs.admon.gui.common.tools.UnitTk;
import javax.swing.DefaultCellEditor;
import javax.swing.JOptionPane;
import javax.swing.JTextField;



public class ValueUnitCellEditor extends DefaultCellEditor
{
   private static final long serialVersionUID = 1L;

   public ValueUnitCellEditor(JTextField textField)
   {
      super(textField);
   }

   @Override
   public boolean stopCellEditing()
   {
      if(!UnitTk.strToValueUnitOfSize(getCellEditorValue().toString()).isValid() )
      {
         JOptionPane.showMessageDialog(null, "The input is invalid. Valid units are Byte, KiB," +
            "MiB, GiB, TiB." + System.lineSeparator() + "Example: 500 MiB", "Invalid value",
            JOptionPane.ERROR_MESSAGE);
         return false;
      }
      
      fireEditingStopped();
      return true;
   }
}

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package com.beegfs.admon.gui.components.tables;

import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import java.awt.Component;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.DefaultTableCellRenderer;


public class InstallTableCellRenderer extends DefaultTableCellRenderer {

    public InstallTableCellRenderer() {
        super();
    }

    @Override
    public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column) {
        String valueStr = "";
        if (value != null)
        {
             valueStr = value.toString();
        }
        setText(valueStr);

        this.setHorizontalAlignment(JLabel.LEFT);
        //String type = (String)table.getModel().getValueAt(row, 0);
        String type = (String)table.getValueAt(row, 0);
        if (type.equals(NodeTypesEnum.MANAGMENT.type()))
        {
            this.setBackground(NodeTypesEnum.MANAGMENT.color());
        }
        else if (type.equals(NodeTypesEnum.METADATA.type()))
        {
            this.setBackground(NodeTypesEnum.METADATA.color());
        }
        else if (type.equals(NodeTypesEnum.STORAGE.type()))
        {
            this.setBackground(NodeTypesEnum.STORAGE.color());
        }
        else if (type.equals(NodeTypesEnum.CLIENT.type()))
        {
            this.setBackground(NodeTypesEnum.CLIENT.color());
        }

        this.setVisible(true);
        this.setToolTipText(valueStr);
        return this;
    }

}

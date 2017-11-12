/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package com.beegfs.admon.gui.components.tables;

import java.awt.Component;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.TableCellRenderer;


public class CenteredTableCellRenderer extends JLabel implements TableCellRenderer {

    public CenteredTableCellRenderer() {
        super();
    }

    @Override
    public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column) {
        String text = (String) value;
        setText(text);
        this.setHorizontalAlignment(JLabel.CENTER);
        return this;
    }

}

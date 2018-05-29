package com.beegfs.admon.gui.components.frames;


import com.beegfs.admon.gui.common.enums.MetaOpsEnum;
import com.beegfs.admon.gui.common.enums.StatsTypeEnum;
import com.beegfs.admon.gui.program.Main;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_METADATA;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_STORAGE;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_USER_METADATA;
import com.beegfs.admon.gui.common.enums.StorageOpsEnum;
import com.beegfs.admon.gui.components.internalframes.stats.JInternalFrameStats;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.GridLayout;
import javax.swing.JCheckBox;
import javax.swing.JPanel;



public class JFrameStatsFilter extends javax.swing.JFrame
{
   private static final long serialVersionUID = 1L;

   private static final int DEFAULT_LAYOUT_GAP_X = 10;
   private static final int DEFAULT_LAYOUT_GAP_Y = 10;
   private static final int DEFAULT_COLUMN_COUNT = 5;
   private static final int DEFAULT_FRAME_SIZE_X = 550;
   private static final int DEFAULT_FRAME_SIZE_Y_META = 450;
   private static final int DEFAULT_FRAME_SIZE_Y_STORAGE = 250;
   private final JInternalFrameStats parent;
   private final StatsTypeEnum type;


   public JFrameStatsFilter(JInternalFrameStats parentFrame)
   {
      this.parent = parentFrame;
      this.type = parent.getType();

      initComponents();

      String titleDataType;
      if (this.type == STATS_CLIENT_METADATA || this.type == STATS_USER_METADATA)
      {
         titleDataType = "Metadata";
         this.setSize(new Dimension(DEFAULT_FRAME_SIZE_X, DEFAULT_FRAME_SIZE_Y_META) );
         this.setPreferredSize(new Dimension(DEFAULT_FRAME_SIZE_X, DEFAULT_FRAME_SIZE_Y_META) );
      }
      else
      {
         titleDataType = "Storage";
         this.setSize(new Dimension(DEFAULT_FRAME_SIZE_X, DEFAULT_FRAME_SIZE_Y_STORAGE) );
         this.setPreferredSize(new Dimension(DEFAULT_FRAME_SIZE_X, DEFAULT_FRAME_SIZE_Y_STORAGE) );
      }

      String tileType;
      if (this.type == STATS_CLIENT_METADATA || this.type == STATS_CLIENT_STORAGE)
      {
         tileType = "Client";
      }
      else
      {
         tileType = "User";
      }

      this.setTitle(tileType + " statistics filter - " + titleDataType);

      initFilterItems();
   }

   private void initFilterItems()
   {
      int index = 0;
      boolean[] filterSelected = parent.getFilterSelected();

      if (this.type == STATS_CLIENT_METADATA || this.type == STATS_USER_METADATA)
      {
         int itemCount = MetaOpsEnum.values().length;
         int rowCount = itemCount / DEFAULT_COLUMN_COUNT;
         if (itemCount % DEFAULT_COLUMN_COUNT != 0 )
         {
            rowCount++;
         }
         this.jPanelFilters.setLayout(new GridLayout(rowCount, DEFAULT_COLUMN_COUNT,
            DEFAULT_LAYOUT_GAP_X, DEFAULT_LAYOUT_GAP_Y));

         for (MetaOpsEnum ops: MetaOpsEnum.values())
         {
            JPanelFilterItem newPanel = new JPanelFilterItem(ops.ordinal(),
                                                             ops.getLabel(),
                                                             filterSelected[index]);
            this.jPanelFilters.add(newPanel);
            index++;
         }
      }
      else
      {
         int itemCount = StorageOpsEnum.values().length;
         int rowCount = itemCount / DEFAULT_COLUMN_COUNT;
         if (itemCount % DEFAULT_COLUMN_COUNT != 0 )
         {
            rowCount++;
         }
         this.jPanelFilters.setLayout(new GridLayout(rowCount, DEFAULT_COLUMN_COUNT,
            DEFAULT_LAYOUT_GAP_X, DEFAULT_LAYOUT_GAP_Y));

         for (StorageOpsEnum ops: StorageOpsEnum.values())
         {
            JPanelFilterItem newPanel = new JPanelFilterItem(ops.ordinal(),
                                                             ops.getLabel(),
                                                             filterSelected[index]);
            this.jPanelFilters.add(newPanel);
            index++;
         }
      }
   }

   private void createAndStoreNewFilter()
   {
      int counter = 0;
      int indexNewFilter = 0;
      int indexFilterSelected = 0;

      boolean[] filterSelected = new boolean[parent.getColumnCountForTableByType()];

      //count the size of the new filter
      for (Object item : jPanelFilters.getComponents())
      {
         JPanelFilterItem filterItem = (JPanelFilterItem) item;
         if (filterItem.isSelected())
         {
            counter++;
         }
      }

      // add the items to the new filter
      int[] newFilter = new int[counter];
      for (Object item : jPanelFilters.getComponents())
      {
         JPanelFilterItem filterItem = (JPanelFilterItem) item;
         if (filterItem.isSelected())
         {
            filterSelected[indexFilterSelected] = true;
            newFilter[indexNewFilter] = filterItem.getID();
            indexNewFilter++;
         }
         else
         {
            filterSelected[indexFilterSelected] = false;
         }
         indexFilterSelected++;
      }

      parent.updateFilterMask(newFilter, filterSelected);
   }

   private static class JPanelFilterItem extends JPanel
   {
      private static final long serialVersionUID = 1L;

      private final int id;
      private final JCheckBox itemCheckBox;

      JPanelFilterItem(int id, String filterName, boolean selected)
      {
         this.id = id;
         this.itemCheckBox = new JCheckBox(filterName, selected);
         this.add(this.itemCheckBox);
         this.setLayout(new FlowLayout(FlowLayout.LEFT));
      }

      public int getID()
      {
         return this.id;
      }

      public boolean isSelected()
      {
         return this.itemCheckBox.isSelected();
      }

      public String getFilterName()
      {
         return this.itemCheckBox.getText();
      }
   }
   /**
    * This method is called from within the constructor to initialize the form. WARNING: Do NOT
    * modify this code. The content of this method is always regenerated by the Form Editor.
    */
   @SuppressWarnings("unchecked")
   // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
   private void initComponents()
   {

      jPanelFrame = new javax.swing.JPanel();
      jPanelFilters = new javax.swing.JPanel();
      jPanelButtons = new javax.swing.JPanel();
      jButtonOk = new javax.swing.JButton();
      fillerButtons = new javax.swing.Box.Filler(new java.awt.Dimension(200, 50), new java.awt.Dimension(200, 50), new java.awt.Dimension(32767, 0));
      jButtonCancel = new javax.swing.JButton();

      setDefaultCloseOperation(javax.swing.WindowConstants.DISPOSE_ON_CLOSE);
      setAlwaysOnTop(true);
      setMinimumSize(new java.awt.Dimension(400, 200));
      setPreferredSize(new java.awt.Dimension(400, 200));
      setResizable(false);
      getContentPane().setLayout(new java.awt.BorderLayout(10, 10));

      jPanelFrame.setLayout(new java.awt.BorderLayout());

      jPanelFilters.setAlignmentX(0.0F);
      jPanelFilters.setAlignmentY(0.0F);
      jPanelFilters.setLayout(new java.awt.GridLayout(1, 0));
      jPanelFrame.add(jPanelFilters, java.awt.BorderLayout.CENTER);

      jButtonOk.setText(Main.getLocal().getString("OK"));
      jButtonOk.setMaximumSize(new java.awt.Dimension(54, 50));
      jButtonOk.setPreferredSize(new java.awt.Dimension(80, 30));
      jButtonOk.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonOkActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonOk);

      fillerButtons.setRequestFocusEnabled(false);
      jPanelButtons.add(fillerButtons);

      jButtonCancel.setText(Main.getLocal().getString("Cancel"));
      jButtonCancel.setPreferredSize(new java.awt.Dimension(80, 30));
      jButtonCancel.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonCancelActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonCancel);

      jPanelFrame.add(jPanelButtons, java.awt.BorderLayout.SOUTH);

      getContentPane().add(jPanelFrame, java.awt.BorderLayout.CENTER);

      pack();
   }// </editor-fold>//GEN-END:initComponents

   private void jButtonOkActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonOkActionPerformed
   {//GEN-HEADEREND:event_jButtonOkActionPerformed
      this.createAndStoreNewFilter();
      this.dispose();
   }//GEN-LAST:event_jButtonOkActionPerformed

   private void jButtonCancelActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonCancelActionPerformed
   {//GEN-HEADEREND:event_jButtonCancelActionPerformed
      this.dispose();
   }//GEN-LAST:event_jButtonCancelActionPerformed

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.Box.Filler fillerButtons;
   private javax.swing.JButton jButtonCancel;
   private javax.swing.JButton jButtonOk;
   private javax.swing.JPanel jPanelButtons;
   private javax.swing.JPanel jPanelFilters;
   private javax.swing.JPanel jPanelFrame;
   // End of variables declaration//GEN-END:variables
}

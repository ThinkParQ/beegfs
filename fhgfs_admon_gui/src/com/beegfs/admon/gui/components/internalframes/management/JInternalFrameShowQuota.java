package com.beegfs.admon.gui.components.internalframes.management;


import com.beegfs.admon.gui.common.enums.quota.GetQuotaEnum;
import com.beegfs.admon.gui.common.enums.quota.QuotaIDTypeEnum;
import com.beegfs.admon.gui.common.enums.quota.SetQuotaIDListEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.ProgressBarThread;
import com.beegfs.admon.gui.common.threading.UpdateThread;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.program.Main;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.table.AbstractTableModel;



public class JInternalFrameShowQuota extends javax.swing.JInternalFrame implements
   JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameStartStop.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "ShowQuotaGui";

   private static final String BASE_URL = HttpTk.generateAdmonUrl("/XML_GetQuota");

   private transient ProgressBarThread pBarThread;

   private final Lock dataLock;
   private Object[][] dataUser;
   private Object[][] dataGroup;

   private transient GetQuotaUpdateThread quotaThread;

   /**
    * Creates new form JInternalFrameShowQuota
    */
   public JInternalFrameShowQuota()
   {
      initComponents();

      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());

      dataLock = new ReentrantLock();
      initData();
      
      changeTabelModel();
   }

   private void initData()
   {
      dataLock.lock();
      try
      {
         int columnCount = jTableQuota.getModel().getColumnCount();
         dataUser = new Object[1][columnCount];
         dataGroup = new Object[1][columnCount];

         initDataStoreUnlock(dataUser);
         initDataStoreUnlock(dataGroup);
      }
      finally
      {
         dataLock.unlock();
      }
   }

   private void initDataStoreUnlock(Object[][] dataStore)
   {
      int rowCount = dataStore.length;
      int columnCount = dataStore[0].length;

      for(int rowIndex = 0; rowIndex < rowCount; rowIndex++)
      {
         for(int columnIndex = 0; columnIndex < columnCount; columnIndex++)
         {
            dataStore[rowIndex][columnIndex] = "";
         }
      }
   }

   private void requestQuota()
   {
      quotaThread = new GetQuotaUpdateThread(BASE_URL, title);
      quotaThread.start();
   }

   private class GetQuotaUpdateThread extends UpdateThread
   {
      private static final long serialVersionUID = 1L;
      
      GetQuotaUpdateThread(String parserUrl, String name)
      {
         super(parserUrl, true, name);
      }

      private String getURLVariablesPart()
      {
         String retVal = "";
         if(jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex() )
         {
            retVal += "?idType=user";
         }
         else
         {
            retVal += "?idType=group";
         }

         if(jCheckBoxZeroValue.isSelected() )
         {
            retVal += "&showZeros=true";
         }
         else
         {
            retVal += "&showZeros=false";
         }

         if(jCheckBoxSystemUser.isEnabled() && jCheckBoxSystemUser.isSelected() )
         {
            retVal += "&showSystemUser=true";
         }
         else
         {
            retVal += "&showSystemUser=false";
         }

         return retVal;
      }

      @Override
      public void run()
      {
         pBarThread = new ProgressBarThread(jProgressBarLoadData, Main.getLocal().getString("Load quota ..."), THREAD_NAME  +
            Main.getLocal().getString("ProgressBar"));
         pBarThread.start();

         jComboBoxIDType.setEnabled(false);
         jButtonRefresh.setEnabled(false);

         lock.lock();
         try
         {
            parser.setUrl(BASE_URL + getURLVariablesPart());
            parser.update();

            while (!stop && (parser.getTreeMap().isEmpty()))
            {
               gotData.await();
            }

            dataLock.lock();
            try
            {
               updateDataSourceUnlocked(parser.getQuotaData() );
            }
            catch (CommunicationException ex)
            {
               LOGGER.log(Level.FINEST, Main.getLocal().getString("Internal error."), ex);
            }
            finally
            {
               dataLock.unlock();
            }
         }
         catch (InterruptedException ex)
         {
            LOGGER.log(Level.FINEST, Main.getLocal().getString("Internal error."), ex);
         }
         catch (CommunicationException ex)
         {
            LOGGER.log(Level.FINEST, Main.getLocal().getString("Communication error."), ex);
         }
         finally
         {
            lock.unlock();

            jComboBoxIDType.setEnabled(true);
            jButtonRefresh.setEnabled(true);
         }

         ((AbstractTableModel)jTableQuota.getModel()).fireTableDataChanged();

         pBarThread.shouldStop();
         jTableQuota.revalidate();
      }

      @Override
      public void shouldStop()
      {
         super.shouldStop();
         parser.shouldStop();
      }

      @Override
      public void start()
      {
         super.start();
      }
   }
   
   private class ShowQuotaTableModel extends AbstractTableModel
   {
      private static final long serialVersionUID = 1L;
      
      @Override
      public int getRowCount()
      {
         int value;

         dataLock.lock();
         try
         {
            value = selectDataSourceUnlocked().length;
         }
         finally
         {
            dataLock.unlock();
         }
         return value;
      }

      @Override
      public int getColumnCount()
      {
         return SetQuotaIDListEnum.getColumnCount();
      }

      @Override
      public String getColumnName(int col)
      {
         String retVal;
         
         if (jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex())
         {
            retVal = GetQuotaEnum.getColumnNames(QuotaIDTypeEnum.USER)[col];
         }
         else
         {
            retVal = GetQuotaEnum.getColumnNames(QuotaIDTypeEnum.GROUP)[col];
         }

         return retVal;
      }

      @Override
      public boolean isCellEditable(int row, int col)
      {
         return false;
      }

      @Override
      public Object getValueAt(int rowIndex, int columnIndex)
      {
         Object value;

         dataLock.lock();
         try
         {
            value = selectDataSourceUnlocked()[rowIndex][columnIndex];

            if (value == null)
            {
               value = "";
            }
         }
         finally
         {
            dataLock.unlock();
         }
         return value;
      }
   }

   @SuppressWarnings("ReturnOfCollectionOrArrayField")
   private Object[][] selectDataSourceUnlocked()
   {
      if(jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex() )
      {
         return dataUser;
      }
      else
      {
         return dataGroup;
      }
   }

   @SuppressWarnings("AssignmentToCollectionOrArrayFieldFromParameter")
   private void updateDataSourceUnlocked(Object[][] newData)
   {
      if(jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex() )
      {
         dataUser = newData;
      }
      else
      {
         dataGroup = newData;
      }
   }

   private void changeTabelModel()
   {
      if(jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex() )
      {
         jCheckBoxSystemUser.setVisible(true);
      }
      else
      {
         jCheckBoxSystemUser.setVisible(false);
      }

      ((AbstractTableModel) jTableQuota.getModel()).fireTableStructureChanged();
      ((AbstractTableModel) jTableQuota.getModel()).fireTableDataChanged();
      jScrollPaneTable.revalidate();
   }
   
   /**
    * This method is called from within the constructor to initialize the form. WARNING: Do NOT
    * modify this code. The content of this method is always regenerated by the Form Editor.
    */
   @SuppressWarnings("unchecked")
   // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
   private void initComponents()
   {
      java.awt.GridBagConstraints gridBagConstraints;

      jPanelFrame = new javax.swing.JPanel();
      jPanelHeader = new javax.swing.JPanel();
      jPanelControll = new javax.swing.JPanel();
      jComboBoxIDType = new javax.swing.JComboBox<>();
      jCheckBoxZeroValue = new javax.swing.JCheckBox();
      jCheckBoxSystemUser = new javax.swing.JCheckBox();
      jButtonRefresh = new javax.swing.JButton();
      jProgressBarLoadData = new javax.swing.JProgressBar();
      jScrollPaneTable = new javax.swing.JScrollPane();
      jTableQuota = new javax.swing.JTable();

      FormListener formListener = new FormListener();

      addInternalFrameListener(formListener);

      jPanelFrame.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jPanelHeader.setLayout(new java.awt.BorderLayout());

      jPanelControll.setLayout(new java.awt.GridBagLayout());

      jComboBoxIDType.setModel(new javax.swing.DefaultComboBoxModel<>(com.beegfs.admon.gui.common.enums.quota.QuotaIDTypeEnum.getValuesForComboBox()));
      jComboBoxIDType.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelControll.add(jComboBoxIDType, gridBagConstraints);

      jCheckBoxZeroValue.setText(Main.getLocal().getString("Show quota with 0 value"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelControll.add(jCheckBoxZeroValue, gridBagConstraints);

      jCheckBoxSystemUser.setText(Main.getLocal().getString("Show system users"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 15);
      jPanelControll.add(jCheckBoxSystemUser, gridBagConstraints);

      jButtonRefresh.setText(Main.getLocal().getString("Refresh quota"));
      jButtonRefresh.setMaximumSize(new java.awt.Dimension(130, 30));
      jButtonRefresh.setMinimumSize(new java.awt.Dimension(130, 30));
      jButtonRefresh.setPreferredSize(new java.awt.Dimension(130, 30));
      jButtonRefresh.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 3;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 15, 5, 5);
      jPanelControll.add(jButtonRefresh, gridBagConstraints);

      jPanelHeader.add(jPanelControll, java.awt.BorderLayout.CENTER);
      jProgressBarLoadData.setString(Main.getLocal().getString("Load quota ..."));
      jProgressBarLoadData.setStringPainted(true);
      jPanelHeader.add(jProgressBarLoadData, java.awt.BorderLayout.NORTH);

      jPanelFrame.add(jPanelHeader, java.awt.BorderLayout.NORTH);

      jTableQuota.setModel(new ShowQuotaTableModel());
      jScrollPaneTable.setViewportView(jTableQuota);

      jPanelFrame.add(jScrollPaneTable, java.awt.BorderLayout.CENTER);

      getContentPane().add(jPanelFrame, java.awt.BorderLayout.CENTER);

      pack();
   }

   // Code for dispatching events from components to event handlers.

   private class FormListener implements java.awt.event.ActionListener, javax.swing.event.InternalFrameListener
   {
      FormListener() {}
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         if (evt.getSource() == jComboBoxIDType)
         {
            JInternalFrameShowQuota.this.jComboBoxIDTypeActionPerformed(evt);
         }
         else if (evt.getSource() == jButtonRefresh)
         {
            JInternalFrameShowQuota.this.jButtonRefreshActionPerformed(evt);
         }
      }

      public void internalFrameActivated(javax.swing.event.InternalFrameEvent evt)
      {
      }

      public void internalFrameClosed(javax.swing.event.InternalFrameEvent evt)
      {
         if (evt.getSource() == JInternalFrameShowQuota.this)
         {
            JInternalFrameShowQuota.this.formInternalFrameClosed(evt);
         }
      }

      public void internalFrameClosing(javax.swing.event.InternalFrameEvent evt)
      {
      }

      public void internalFrameDeactivated(javax.swing.event.InternalFrameEvent evt)
      {
      }

      public void internalFrameDeiconified(javax.swing.event.InternalFrameEvent evt)
      {
      }

      public void internalFrameIconified(javax.swing.event.InternalFrameEvent evt)
      {
      }

      public void internalFrameOpened(javax.swing.event.InternalFrameEvent evt)
      {
      }
   }// </editor-fold>//GEN-END:initComponents

   private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt)//GEN-FIRST:event_formInternalFrameClosed
   {//GEN-HEADEREND:event_formInternalFrameClosed
      FrameManager.delFrame(this);
   }//GEN-LAST:event_formInternalFrameClosed

   private void jButtonRefreshActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonRefreshActionPerformed
   {//GEN-HEADEREND:event_jButtonRefreshActionPerformed
      requestQuota();
   }//GEN-LAST:event_jButtonRefreshActionPerformed

   private void jComboBoxIDTypeActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxIDTypeActionPerformed
   {//GEN-HEADEREND:event_jComboBoxIDTypeActionPerformed
      changeTabelModel();
   }//GEN-LAST:event_jComboBoxIDTypeActionPerformed

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JButton jButtonRefresh;
   private javax.swing.JCheckBox jCheckBoxSystemUser;
   private javax.swing.JCheckBox jCheckBoxZeroValue;
   private javax.swing.JComboBox<String> jComboBoxIDType;
   private javax.swing.JPanel jPanelControll;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelHeader;
   private javax.swing.JProgressBar jProgressBarLoadData;
   private javax.swing.JScrollPane jScrollPaneTable;
   private javax.swing.JTable jTableQuota;
   // End of variables declaration//GEN-END:variables

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameShowQuota;
   }

   @Override
   public final String getFrameTitle()
   {
      return Main.getLocal().getString("Show quota");
   }
}

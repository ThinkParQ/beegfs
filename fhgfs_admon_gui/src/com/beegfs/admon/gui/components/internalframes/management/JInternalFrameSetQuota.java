package com.beegfs.admon.gui.components.internalframes.management;


import com.beegfs.admon.gui.common.XMLParser;
import com.beegfs.admon.gui.common.enums.UpdateDataTypeEnum;
import com.beegfs.admon.gui.common.enums.quota.QuotaIDTypeEnum;
import com.beegfs.admon.gui.common.enums.quota.QuotaQueryTypeEnum;
import static com.beegfs.admon.gui.common.enums.quota.QuotaQueryTypeEnum.SET_QUOTA_ID_LIST;
import com.beegfs.admon.gui.common.enums.quota.SetQuotaIDListEnum;
import com.beegfs.admon.gui.common.enums.quota.SetQuotaIDRangeEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.ProgressBarThread;
import com.beegfs.admon.gui.common.threading.UpdateThread;
import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.frames.JFrameFileChooser;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameUpdateableInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.components.tables.ValueUnitCellEditor;
import com.beegfs.admon.gui.program.Main;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.StringTokenizer;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JOptionPane;
import javax.swing.JTextField;
import javax.swing.table.AbstractTableModel;



public class JInternalFrameSetQuota extends javax.swing.JInternalFrame implements
   JInternalFrameUpdateableInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameStartStop.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "SetQuotaGui";

   private static final int DEFAULT_ROW_COUNT = 20;
   private static final int ERROR_LINES_PER_ROW = 10;

   private static final String BASE_URL = HttpTk.generateAdmonUrl("/XML_SetQuota");

   private transient ProgressBarThread pBarThread;

   private final Lock dataLock;
   private ArrayList<ArrayList<Object>> dataUserList;
   private ArrayList<ArrayList<Object>> dataUserRange;
   private ArrayList<ArrayList<Object>> dataGroupList;
   private ArrayList<ArrayList<Object>> dataGroupRange;

   private transient SetQuotaUpdateThread quotaThread;

   /**
    * Creates new form JInternalFrameSetQuota
    */
   public JInternalFrameSetQuota()
   {
      initComponents();

      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());

      dataLock = new ReentrantLock();
      initData();

      jTableQuota.setDefaultEditor(com.beegfs.admon.gui.common.ValueUnit.class,
         new ValueUnitCellEditor(new JTextField() ) );

      changeTabelModel();
   }

   private void initData()
   {
      dataLock.lock();
      try
      {
         dataUserList = new ArrayList<>(DEFAULT_ROW_COUNT);
         initDataStore(dataUserList);
         dataUserRange = new ArrayList<>(DEFAULT_ROW_COUNT);
         initDataStore(dataUserRange);
         dataGroupList = new ArrayList<>(DEFAULT_ROW_COUNT);
         initDataStore(dataGroupList);
         dataGroupRange = new ArrayList<>(DEFAULT_ROW_COUNT);
         initDataStore(dataGroupRange);
      }
      finally
      {
         dataLock.unlock();
      }
   }

   private void initDataStore(ArrayList<ArrayList<Object>> dataStore)
   {
      int columnCount = jTableQuota.getModel().getColumnCount();
      ArrayList<Object> columnData = new ArrayList<>(columnCount);
      
      for(int index = 0; index < columnCount; index++)
      {
         columnData.add("");
      }

      for(int index = 0; index < DEFAULT_ROW_COUNT; index++)
      {
         dataStore.add(new ArrayList<>(columnData));
      }
   }

   private void requestQuotaLimits()
   {
      quotaThread = new SetQuotaUpdateThread(BASE_URL, title, false);
      quotaThread.start();
   }

   private void updateQuotaLimits()
   {
      InputStream serverResp;
      String url = HttpTk.generateAdmonUrl("/XML_SetQuota");

      int rowCount = jTableQuota.getModel().getRowCount();
      int columnCount = jTableQuota.getModel().getColumnCount();

      int objSize = (columnCount * rowCount);
      Object[] obj = new Object[objSize];
      int objCounter = 0;

      dataLock.lock();
      try
      {
         ArrayList<ArrayList<Object>> tmpData = selectDataSourceUnlocked();
         for (ArrayList<Object> row : tmpData)
         {
            int columnIndex = 0;

            for (Object value : row)
            {
               obj[objCounter] = "meta";
               objCounter++;

               obj[objCounter] = value;
               objCounter++;

               columnIndex++;
            }
         }

         serverResp = HttpTk.sendPostData(url, obj);
         XMLParser parser = new XMLParser(THREAD_NAME);
         parser.readFromInputStream(serverResp);
         ArrayList<String> errors = parser.getVector("errors");

         if (!errors.isEmpty())
         {
            markInvalidRows(errors);

            JOptionPane.showMessageDialog(null, Main.getLocal().getString("The marked rows are invalid and this quota ") +
            	Main.getLocal().getString("limits are ignored."), Main.getLocal().getString("Errors occured"), JOptionPane.ERROR_MESSAGE);
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
         {
            e,
            true
         });
      }
      finally
      {
         dataLock.unlock();
      } 
   }

   private void loadQuotaLimitsFromFile()
   {
      JFrameFileChooser frame = new JFrameFileChooser(this, UpdateDataTypeEnum.QUOTA);
      frame.setVisible(true);   
   }

   private void markInvalidRows(ArrayList<String> errors)
   {
      
   }

   @Override
   public void updateData(ArrayList<String> data, UpdateDataTypeEnum type)
   {
      ArrayList<Integer> failedLines = new ArrayList<>(DEFAULT_ROW_COUNT);

      dataLock.lock();
      try
      {
         int lineNumber = 0;
         int columnCount = jTableQuota.getModel().getColumnCount();

         for (String line : data)
         {
            StringTokenizer tokenizer = new StringTokenizer(line, ",", false);

            if(tokenizer.countTokens() != columnCount)
            {
               failedLines.add(lineNumber);
            }

            ArrayList<Object> newRow = new ArrayList<>(columnCount);
            selectDataSourceUnlocked().add(newRow);
            while(tokenizer.hasMoreTokens() )
            {
               newRow.add(tokenizer.nextToken() );
            }

            lineNumber++;
         }
      }
      finally
      {
         dataLock.unlock();
      }

      ((AbstractTableModel)jTableQuota.getModel()).fireTableDataChanged();
      jTableQuota.revalidate();

      if(!failedLines.isEmpty())
      {
         int valueCounter = 0;
         StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
         errorStr.append(Main.getLocal().getString("Some lines of the file are not valid:"));
         errorStr.append(System.lineSeparator());
         for (int failedLine : failedLines)
         {
            errorStr.append(failedLine);
            errorStr.append(", ");
            valueCounter++;

            if (valueCounter == ERROR_LINES_PER_ROW)
            {
               errorStr.append(System.lineSeparator());
               valueCounter = 0;
            }
         }

         JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                    JOptionPane.ERROR_MESSAGE);
      }
   }

   private String getURLVariablesPart(boolean updateLimits)
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

      if(updateLimits)
      {
         retVal += "&updateLimits=true";

         if(jComboBoxQueryType.getSelectedIndex() == SET_QUOTA_ID_LIST.getIndex())
         {
            retVal += "&queryType=list";
         }
         else
         {
            retVal += "&queryType=range";
         }
      }
      else
      {
         retVal += "&updateLimits=false";
         retVal += "&queryType=list";
      }

      return retVal;
   }

   private class SetQuotaUpdateThread extends UpdateThread
   {
      private static final long serialVersionUID = 1L;
      
      private final boolean updateLimits;

      SetQuotaUpdateThread(String parserUrl, String name, boolean updateLimits)
      {
         super(parserUrl, true, name);
         this.updateLimits = updateLimits;
      }

      @Override
      public void run()
      {
       pBarThread = new ProgressBarThread(jProgressBarLoadData, Main.getLocal().getString("Load quota ..."), THREAD_NAME  +
      Main.getLocal().getString("ProgressBar"));
         pBarThread.start();

         jComboBoxIDType.setEnabled(false);
         jComboBoxQueryType.setEnabled(false);
         jButtonLoad.setEnabled(false);
         jButtonSave.setEnabled(false);

         lock.lock();
         try
         {
            parser.setUrl(BASE_URL + getURLVariablesPart(updateLimits));
            parser.update();

            while (!stop && (parser.getTreeMap().isEmpty()))
            {
               gotData.await();
            }

            dataLock.lock();
            try
            {
               updateDataSourceUnlocked(parser.getQuotaLimits() );
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
            jComboBoxQueryType.setEnabled(true);
            jButtonLoad.setEnabled(true);
            jButtonSave.setEnabled(true);
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

   private class SetQuotaTableModel extends AbstractTableModel
   {
      private static final long serialVersionUID = 1L;

      private final Class<?>[] columnTypesIDList =
      {
         java.lang.String.class,
         com.beegfs.admon.gui.common.ValueUnit.class,
         java.lang.Integer.class
      };

      private final Class<?>[] columnTypesIDRange =
      {
         java.lang.Integer.class,
         java.lang.Integer.class,
         com.beegfs.admon.gui.common.ValueUnit.class,
         java.lang.Integer.class
      };


      @Override
      public Class<?> getColumnClass(int columnIndex)
      {
         dataLock.lock();
         try
         {
            if (jComboBoxQueryType.getSelectedIndex() == SET_QUOTA_ID_LIST.getIndex())
            {
               return columnTypesIDList[columnIndex];
            }
            else
            {
               return columnTypesIDRange[columnIndex];
            }
         }
         finally
         {
            dataLock.unlock();
         }
      }

      @Override
      public int getRowCount()
      {
         int value;
         
         dataLock.lock();
         try
         {
            value = selectDataSourceUnlocked().size();
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
         int retVal;
         if (jComboBoxQueryType.getSelectedIndex() == SET_QUOTA_ID_LIST.getIndex())
         {
            retVal = SetQuotaIDListEnum.getColumnCount();
         }
         else
         {
            retVal =  SetQuotaIDRangeEnum.getColumnCount();
         }

         return retVal;
      }

      @Override
      public String getColumnName(int col)
      {
         String retVal;
         if (jComboBoxQueryType.getSelectedIndex() == SET_QUOTA_ID_LIST.getIndex())
         {
            if (jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex())
            {
               retVal = SetQuotaIDListEnum.getColumnNames(QuotaIDTypeEnum.USER)[col];
            }
            else
            {
               retVal = SetQuotaIDListEnum.getColumnNames(QuotaIDTypeEnum.GROUP)[col];
            }
         }
         else
         {
            if (jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex())
            {
               retVal = SetQuotaIDRangeEnum.getColumnNames(QuotaIDTypeEnum.USER)[col];
            }
            else
            {
               retVal = SetQuotaIDRangeEnum.getColumnNames(QuotaIDTypeEnum.GROUP)[col];
            }
         }
         return retVal;
      }

      @Override
      public boolean isCellEditable(int row, int col)
      {
         return true;
      }

      @Override
      public Object getValueAt(int rowIndex, int columnIndex)
      {
         Object value;

         dataLock.lock();
         try
         {
            value = selectDataSourceUnlocked().get(rowIndex).get(columnIndex);
         }
         finally
         {
            dataLock.unlock();
         }
         
         return value;
      }

      @Override
      public void setValueAt(Object value, int rowIndex, int columnIndex)      {
         dataLock.lock();
         try
         {
            selectDataSourceUnlocked().get(rowIndex).set(columnIndex, value);
         }
         finally
         {
            dataLock.unlock();
         }
         
         fireTableCellUpdated(rowIndex, columnIndex);
      }
   }

   @SuppressWarnings("ReturnOfCollectionOrArrayField")
   private ArrayList<ArrayList<Object>> selectDataSourceUnlocked()
   {
      if (jComboBoxQueryType.getSelectedIndex() == QuotaQueryTypeEnum.SET_QUOTA_ID_LIST.getIndex() )
      {
         if (jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex() )
         {
            return dataUserList;
         }
         else
         {
            return dataGroupList;
         }
      }
      else
      {
         if (jComboBoxIDType.getSelectedIndex() ==  QuotaIDTypeEnum.USER.getIndex() )
         {
            return dataUserRange;
         }
         else
         {
            return dataGroupRange;
         }
      }
   }

   @SuppressWarnings("AssignmentToCollectionOrArrayFieldFromParameter")
   private void updateDataSourceUnlocked(ArrayList<ArrayList<Object>> newData)
   {
      if (jComboBoxQueryType.getSelectedIndex() == QuotaQueryTypeEnum.SET_QUOTA_ID_LIST.getIndex() )
      {
         if (jComboBoxIDType.getSelectedIndex() == QuotaIDTypeEnum.USER.getIndex() )
         {
            dataUserList = newData;
         }
         else
         {
            dataGroupList = newData;
         }
      }
      else
      {
         if (jComboBoxIDType.getSelectedIndex() ==  QuotaIDTypeEnum.USER.getIndex() )
         {
            dataUserRange = newData;
         }
         else
         {
            dataGroupRange = newData;
         }
      }
   }

   private void changeTabelModel()
   {
      if(jComboBoxQueryType.getSelectedIndex() == QuotaQueryTypeEnum.SET_QUOTA_RANGE.getIndex() )
      {
         jButtonLoad.setEnabled(false);
         jButtonLoadFile.setEnabled(false);
      }
      else
      {
         jButtonLoad.setEnabled(true);
         jButtonLoadFile.setEnabled(true);
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
      jScrollPaneTable = new javax.swing.JScrollPane();
      jTableQuota = new javax.swing.JTable();
      jPanelHeader = new javax.swing.JPanel();
      jPanelControll = new javax.swing.JPanel();
      jComboBoxIDType = new javax.swing.JComboBox<>();
      jComboBoxQueryType = new javax.swing.JComboBox<>();
      jButtonLoad = new javax.swing.JButton();
      jButtonLoadFile = new javax.swing.JButton();
      jButtonSave = new javax.swing.JButton();
      jProgressBarLoadData = new javax.swing.JProgressBar();

      FormListener formListener = new FormListener();

      addInternalFrameListener(formListener);

      jPanelFrame.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jTableQuota.setModel(new SetQuotaTableModel()
      );
      jScrollPaneTable.setViewportView(jTableQuota);

      jPanelFrame.add(jScrollPaneTable, java.awt.BorderLayout.CENTER);

      jPanelHeader.setLayout(new java.awt.BorderLayout());

      jPanelControll.setLayout(new java.awt.GridBagLayout());

      jComboBoxIDType.setModel(new javax.swing.DefaultComboBoxModel<>(com.beegfs.admon.gui.common.enums.quota.QuotaIDTypeEnum.getValuesForComboBox()));
      jComboBoxIDType.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelControll.add(jComboBoxIDType, gridBagConstraints);

      jComboBoxQueryType.setModel(new javax.swing.DefaultComboBoxModel<>(com.beegfs.admon.gui.common.enums.quota.QuotaQueryTypeEnum.getValuesForComboBoxSetQuota()));
      jComboBoxQueryType.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 15);
      jPanelControll.add(jComboBoxQueryType, gridBagConstraints);

      jButtonLoad.setText(Main.getLocal().getString("Load from server"));
      jButtonLoad.setMaximumSize(new java.awt.Dimension(140, 30));
      jButtonLoad.setMinimumSize(new java.awt.Dimension(140, 30));
      jButtonLoad.setPreferredSize(new java.awt.Dimension(140, 30));
      jButtonLoad.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 15, 5, 5);
      jPanelControll.add(jButtonLoad, gridBagConstraints);

      jButtonLoadFile.setText(Main.getLocal().getString("Load from File"));
      jButtonLoadFile.setMaximumSize(new java.awt.Dimension(140, 30));
      jButtonLoadFile.setMinimumSize(new java.awt.Dimension(140, 30));
      jButtonLoadFile.setPreferredSize(new java.awt.Dimension(140, 30));
      jButtonLoadFile.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 3;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 15);
      jPanelControll.add(jButtonLoadFile, gridBagConstraints);

      jButtonSave.setText(Main.getLocal().getString("Set quota limits"));
      jButtonSave.setMaximumSize(new java.awt.Dimension(140, 30));
      jButtonSave.setMinimumSize(new java.awt.Dimension(140, 30));
      jButtonSave.setPreferredSize(new java.awt.Dimension(140, 30));
      jButtonSave.addActionListener(formListener);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 4;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 15, 5, 5);
      jPanelControll.add(jButtonSave, gridBagConstraints);

      jPanelHeader.add(jPanelControll, java.awt.BorderLayout.CENTER);

      jProgressBarLoadData.setString("Load quota ...");
      jProgressBarLoadData.setStringPainted(true);
      jPanelHeader.add(jProgressBarLoadData, java.awt.BorderLayout.NORTH);

      jPanelFrame.add(jPanelHeader, java.awt.BorderLayout.NORTH);

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
            JInternalFrameSetQuota.this.jComboBoxIDTypeActionPerformed(evt);
         }
         else if (evt.getSource() == jComboBoxQueryType)
         {
            JInternalFrameSetQuota.this.jComboBoxQueryTypeActionPerformed(evt);
         }
         else if (evt.getSource() == jButtonLoad)
         {
            JInternalFrameSetQuota.this.jButtonLoadActionPerformed(evt);
         }
         else if (evt.getSource() == jButtonLoadFile)
         {
            JInternalFrameSetQuota.this.jButtonLoadFileActionPerformed(evt);
         }
         else if (evt.getSource() == jButtonSave)
         {
            JInternalFrameSetQuota.this.jButtonSaveActionPerformed(evt);
         }
      }

      public void internalFrameActivated(javax.swing.event.InternalFrameEvent evt)
      {
      }

      public void internalFrameClosed(javax.swing.event.InternalFrameEvent evt)
      {
         if (evt.getSource() == JInternalFrameSetQuota.this)
         {
            JInternalFrameSetQuota.this.formInternalFrameClosed(evt);
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

   private void jButtonSaveActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonSaveActionPerformed
   {//GEN-HEADEREND:event_jButtonSaveActionPerformed
      updateQuotaLimits();
   }//GEN-LAST:event_jButtonSaveActionPerformed

   private void jButtonLoadActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonLoadActionPerformed
   {//GEN-HEADEREND:event_jButtonLoadActionPerformed
      requestQuotaLimits();
   }//GEN-LAST:event_jButtonLoadActionPerformed

   private void jComboBoxQueryTypeActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxQueryTypeActionPerformed
   {//GEN-HEADEREND:event_jComboBoxQueryTypeActionPerformed
      changeTabelModel();
   }//GEN-LAST:event_jComboBoxQueryTypeActionPerformed

   private void jButtonLoadFileActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonLoadFileActionPerformed
   {//GEN-HEADEREND:event_jButtonLoadFileActionPerformed
      loadQuotaLimitsFromFile();
   }//GEN-LAST:event_jButtonLoadFileActionPerformed

   private void jComboBoxIDTypeActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxIDTypeActionPerformed
   {//GEN-HEADEREND:event_jComboBoxIDTypeActionPerformed
      changeTabelModel();
   }//GEN-LAST:event_jComboBoxIDTypeActionPerformed


   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JButton jButtonLoad;
   private javax.swing.JButton jButtonLoadFile;
   private javax.swing.JButton jButtonSave;
   private javax.swing.JComboBox<String> jComboBoxIDType;
   private javax.swing.JComboBox<String> jComboBoxQueryType;
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
      return obj instanceof JInternalFrameSetQuota;
   }

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("Set quota");
   }
}

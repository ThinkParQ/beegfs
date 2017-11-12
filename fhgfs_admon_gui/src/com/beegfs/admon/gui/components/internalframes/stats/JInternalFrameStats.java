package com.beegfs.admon.gui.components.internalframes.stats;

import com.beegfs.admon.gui.common.enums.MetaOpsEnum;
import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import com.beegfs.admon.gui.common.enums.StatsTypeEnum;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_METADATA;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_STORAGE;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_USER_METADATA;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_USER_STORAGE;
import com.beegfs.admon.gui.common.enums.StorageOpsEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.UpdateThread;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.frames.JFrameStatsFilter;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.program.Main;
import java.awt.Color;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JSpinner;
import javax.swing.table.AbstractTableModel;


public class JInternalFrameStats extends javax.swing.JInternalFrame implements
   JInternalFrameInterface
{
   private static final Logger LOGGER = Logger.getLogger(
      JInternalFrameStats.class.getCanonicalName());
   private static final long serialVersionUID = 1L;

   private static final int DEFAULT_LINE_COUNT = 20;
   private static final int MIN_LINE_COUNT = 1;
   private static final int INCREMENT_LINE_COUNT = 1;

   private static final int DEFAULT_INTERVAL = 3;
   private static final int MIN_INTERVAL = 1;
   private static final int INCREMENT_INTERVAL = 1;

   private static final int FIRST_DATA_SEQUENCE_ID = 2; // start with ID 2 because, the first update
                                                        // contains the counter since the last start
                                                        // of the servers

   final Lock lockTableData = new ReentrantLock(true); /* synchronize the table data and the filter
                                                       mask */

   private final String url;
   private final StatsTypeEnum type;
   private int requestorID;
   private long nextDataSequenceID;

   private Object[][] tableData;
   private int[] filterMask;           // contains the mapping from table column to tableData column
   private boolean[] filterSelected;   // true if the column of the tableData is used, rquired by
                                       // JFrameStatsFilter

   private transient StatsUpdateThread updateThread;

   private String updateInterval = String.valueOf(DEFAULT_INTERVAL);
   private String clientCount = String.valueOf(DEFAULT_LINE_COUNT);

   private InetAddress netResolver;

   /**
    * Creates new form JInternalFrameClientStats
    * @param type The stats type, a value from enum StatsTypeEnum
    */
   public JInternalFrameStats(StatsTypeEnum type)
   {
      if ( (type == STATS_CLIENT_METADATA) || (type == STATS_CLIENT_STORAGE) )
      {
         this.url = HttpTk.generateAdmonUrl("/XML_ClientStats?");
      }
      else
      {
         this.url = HttpTk.generateAdmonUrl("/XML_UserStats?");
      }

      this.type = type;

      this.initFilterMask();

      init();
   }

   private void init()
   {
      initComponents();
      adjustColumnWidth();

      ((JSpinner.DefaultEditor)jSpinnerLineCount.getEditor()).getTextField().
         setBackground(Color.WHITE);
      ((JSpinner.DefaultEditor)jSpinnerInterval.getEditor()).getTextField().
         setBackground(Color.WHITE);

      this.nextDataSequenceID = FIRST_DATA_SEQUENCE_ID;
      setFrameIcon(GuiTk.getFrameIcon());

      if ( (type == STATS_CLIENT_METADATA) || (type == STATS_CLIENT_STORAGE) )
      {
    	  jLabelLineCount.setText(Main.getLocal().getString("Number of clients:"));
    	  jPanelUseNames.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Use Hostname")));
         jPanelStatistics.setBorder(javax.swing.BorderFactory.createTitledBorder(
        		 Main.getLocal().getString("Client Statistics")));
      }
      else
      { // translation into user name only possible on the server
    	  jLabelLineCount.setText(Main.getLocal().getString("Number of users:"));
         jPanelUseNames.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Use Usernames")));
         jPanelUseNames.setVisible(false);
         jPanelStatistics.setBorder(javax.swing.BorderFactory.createTitledBorder(
        		 Main.getLocal().getString("User Statistics")));
      }

      // init and start of update thread is done by formInternalFrameOpened
      // stop of update thread is done by formInternalFrameClosed

      this.jButtonSetConfig.setEnabled(false);
   }

   private void initFilterMask()
   {
      this.filterSelected = new boolean[this.getColumnCountForTableByType()];

      if ( (type == STATS_CLIENT_METADATA) || (type == STATS_USER_METADATA) )
      {
         this.filterMask = new int[13];

         int index = 0;
         this.filterMask[index] = MetaOpsEnum.META_OPS_IP.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_IP.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_OPSUM.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_OPSUM.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_MKDIR.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_MKDIR.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_MKFILE.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_MKFILE.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_RMDIR.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_RMDIR.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_OPEN.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_OPEN.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_STAT.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_STAT.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_UNLINK.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_UNLINK.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_LOOKUPINTENT_SIMPLE.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_LOOKUPINTENT_SIMPLE.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_LOOKUPINTENT_STAT.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_LOOKUPINTENT_STAT.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_LOOKUPINTENT_REVALIDATE.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_LOOKUPINTENT_REVALIDATE.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_LOOKUPINTENT_OPEN.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_LOOKUPINTENT_OPEN.ordinal()] = true;
         index++;
         this.filterMask[index] = MetaOpsEnum.META_OPS_LOOKUPINTENT_CREATE.ordinal();
         this.filterSelected[MetaOpsEnum.META_OPS_LOOKUPINTENT_CREATE.ordinal()] = true;
      }
      else
      {
         this.filterMask = new int[8];

         int index = 0;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_IP.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_IP.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_OPSUM.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_OPSUM.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_GETLOCALFILESIZE.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_GETLOCALFILESIZE.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_FSYNCLOCAL.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_FSYNCLOCAL.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_READOPS.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_READOPS.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_READBYTES.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_READBYTES.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_WRITEOPS.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_WRITEOPS.ordinal()] = true;
         index++;
         this.filterMask[index] = StorageOpsEnum.STORAGE_OPS_WRITEBYTES.ordinal();
         this.filterSelected[StorageOpsEnum.STORAGE_OPS_WRITEBYTES.ordinal()] = true;
      }
   }

   /**
    * Returns the column count by the length of the enum of clients stats type, only useful if no
    * filter is set
    */
   public int getColumnCountForTableByType()
   {
      if (type == STATS_CLIENT_METADATA || type == STATS_USER_METADATA)
      {
         return MetaOpsEnum.values().length;
      }
      else
      {
         return StorageOpsEnum.values().length;
      }
   }

   public int getColumnCountForTable()
   {
      int length;

      this.lockTableData.lock();
      try
      {
         if(this.filterMask != null)
         {
            length = this.filterMask.length;
         } else
         {
            length = 0;
         }
      }
      finally
      {
         this.lockTableData.unlock();
      }
      return length;
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      if (!(obj instanceof JInternalFrameStats))
      {
         return false;
      }

      JInternalFrameStats objCasted = (JInternalFrameStats) obj;
      return objCasted.getType().equals(type);
   }

   @Override
   public final String getFrameTitle()
   {
      return type.description();
   }

   public StatsTypeEnum getType()
   {
      return this.type;
   }

   private void startUpdateThread()
   {
      updateThread = new StatsUpdateThread(this.url, type.type() );
      updateThread.start();
   }

   private void stopUpdateThread()
   {
      updateThread.shouldStop();
      nextDataSequenceID = FIRST_DATA_SEQUENCE_ID;
   }

   private void adjustColumnWidth()
   {
      for (int columnId = 0; columnId < jTableStatistics.getColumnCount(); columnId++)
      {
         int columnWidth;

         if (type == STATS_CLIENT_METADATA || type == STATS_USER_METADATA)
         {
            columnWidth = MetaOpsEnum.values()[columnId].getColumnWidth();
         }
         else
         {
            columnWidth = StorageOpsEnum.values()[columnId].getColumnWidth();
         }

         jTableStatistics.getColumnModel().getColumn(columnId).setPreferredWidth(columnWidth);
         jTableStatistics.getColumnModel().getColumn(columnId).setWidth(columnWidth);
      }
   }

   public void updateFilterMask(int[] newFilter, boolean[] newFilterSelected)
   {
      this.lockTableData.lock();
      try
      {
         this.filterMask = newFilter.clone();
         this.filterSelected = newFilterSelected.clone();
      }
      finally
      {
         this.lockTableData.unlock();
      }

      ((AbstractTableModel) jTableStatistics.getModel()).fireTableStructureChanged();
      ((AbstractTableModel) jTableStatistics.getModel()).fireTableDataChanged();
      this.adjustColumnWidth();
   }

   public boolean[] getFilterSelected()
   {
      return this.filterSelected.clone();
   }

   private class StatsUpdateThread extends UpdateThread
   {
      private static final int LOOP_SLEEP_MS = 1000;
      private final String baseUrl;

      StatsUpdateThread(String parserUrl, String name)
      {
         super(parserUrl, true, name);
         baseUrl = parserUrl;
      }

      private String getURLVariablesPart(String updateInterval, String numLines,
         long nextDataSequenceID)
      {
         int nodeType;
         if (type == STATS_CLIENT_METADATA || type == STATS_USER_METADATA)
         {
            nodeType = NodeTypesEnum.METADATA.ordinal();
         }
         else
         {
            nodeType = NodeTypesEnum.STORAGE.ordinal();
         }

         return "nodetype=" + nodeType + "&interval=" + updateInterval +
            "&numLines=" + numLines + "&requestorID=" + requestorID +
            "&nextDataSequenceID=" + nextDataSequenceID;
      }

      @Override
      public void start()
      {
         super.startWithoutParser();
      }

      @Override
      public void run()
      {
         ((AbstractTableModel)jTableStatistics.getModel()).fireTableDataChanged();

         while (!stop)
         {
            try
            {
               boolean updateResult = false;
               lock.lock();
               try
               {
                  parser.setUrl(baseUrl +  getURLVariablesPart(updateInterval, clientCount,
                     nextDataSequenceID) );
                  updateResult = parser.update();
               }
               finally
               {
                  lock.unlock();
               }

               if(updateResult)
               {
                  int newRequestorID = Integer.parseInt(parser.getValue("requestorID"));
                  if (newRequestorID != requestorID)
                  {
                     requestorID = newRequestorID;
                     nextDataSequenceID = FIRST_DATA_SEQUENCE_ID;
                  }

                  long recievedDataSequenceID = Long.parseLong(parser.getValue("dataSequenceID"));

                  if (recievedDataSequenceID < FIRST_DATA_SEQUENCE_ID)
                  {
                     Object[][] data = new Object[1][getColumnCountForTableByType()];
                     data[0][0] = Main.getLocal().getString("waiting for data ...");

                     lockTableData.lock();
                     try
                     {
                        tableData = data;
                     }
                     finally
                     {
                        lockTableData.unlock();
                     }

                     ((AbstractTableModel)jTableStatistics.getModel()).fireTableDataChanged();
                  }
                  else if (recievedDataSequenceID >= nextDataSequenceID)
                  {
                     Object[][] data = parser.getStats(type);

                     lockTableData.lock();
                     try
                     {
                        tableData = data;
                        nextDataSequenceID++;
                     }
                     finally
                     {
                        lockTableData.unlock();
                     }

                     ((AbstractTableModel)jTableStatistics.getModel()).fireTableDataChanged();
                  }
               }

               sleep(LOOP_SLEEP_MS);

            }
            catch (InterruptedException | NullPointerException | NumberFormatException ex)
            {
               LOGGER.log(Level.FINEST, "Internal error.", ex);
            }
            catch (CommunicationException ex)
            {
               LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
               {
                  ex,
                  true
               });
            }
         }
         parser.shouldStop();
      }

      @Override
      public void shouldStop()
      {
         super.shouldStop();
         parser.shouldStop();
      }

   }

   private class StatsTableModel extends AbstractTableModel
   {
      private static final long serialVersionUID = 1L;

      @Override
      public int getRowCount()
      {
         int length;

         lockTableData.lock();
         try
         {
            if(tableData != null)
            {
               length = tableData.length;
            }
            else
            {
               length = 0;
            }
         }
         finally
         {
            lockTableData.unlock();
         }
         return length;
      }

      @Override
      public String getColumnName(int col)
      {
         String retVal;
         lockTableData.lock();
         try
         {
            if (type == STATS_CLIENT_METADATA || type == STATS_USER_METADATA)
            {
               int index = filterMask[col];
               if( (type == STATS_USER_METADATA) && (index == 0) )
               {
                  retVal = "user ID";
               }
               else
               {
                  retVal = MetaOpsEnum.values()[index].getLabel();
               }
            }
            else
            {
               int index = filterMask[col];
               if( (type == STATS_USER_STORAGE) && (index == 0) )
               {
                  retVal = "user ID";
               }
               else
               {
                  retVal = StorageOpsEnum.values()[index].getLabel();
               }
            }
         }
         finally
         {
            lockTableData.unlock();
         }
         return retVal;
      }

      @Override
      public boolean isCellEditable(int row, int col)
      {
         return false;
      }

      @Override
      public int getColumnCount()
      {
         int length;

         lockTableData.lock();
         try
         {
            if (filterMask != null)
            {
               length = filterMask.length;
            }
            else
            {
               length = 0;
            }
         }
         finally
         {
            lockTableData.unlock();
         }
         return length;
      }

      @Override
      public Object getValueAt(int rowIndex, int columnIndex)
      {
         Object value;

         lockTableData.lock();
         try
         {
            if (tableData != null)
            {
               int filteredColumnIndex = filterMask[columnIndex];

               if ((rowIndex >= tableData.length) || (filteredColumnIndex >= tableData[0].length))
               {
                  value = "";
               }
               else
               {
                  value = tableData[rowIndex][filteredColumnIndex];
                  if (value == null)
                  {
                     value = "";
                  }
                  else if (columnIndex == 0 && jCheckBoxUseNames.isSelected())
                  {
                     String valueStr = value.toString();
                     if (rowIndex == 0) // sum row or notification row
                     {
                        value = valueStr;
                     }
                     else
                     {
                        try
                        {
                           netResolver = InetAddress.getByName(valueStr);
                           value = netResolver.getHostName();
                        }
                        catch (UnknownHostException ex)
                        {
                           LOGGER.log(Level.FINEST, "Unknown host: {0}", valueStr);
                           value = valueStr;
                        }
                     }
                  }
                  else
                  {
                     String valueStr = value.toString();
                     if (valueStr.equals("0") && columnIndex != 0)
                     {
                        value = "";
                     }
                  }
               }
            }
            else
            {
               value = "";
            }
         }
         finally
         {
            lockTableData.unlock();
         }
         return value;
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

      jScrollPaneWindow = new javax.swing.JScrollPane();
      jPanelWindow = new javax.swing.JPanel();
      jPanelSettings = new javax.swing.JPanel();
      jPanelConfig = new javax.swing.JPanel();
      jPanelLineCount = new javax.swing.JPanel();
      jSpinnerLineCount = new javax.swing.JSpinner();
      jLabelLineCount = new javax.swing.JLabel();
      jPanelInterval = new javax.swing.JPanel();
      jSpinnerInterval = new javax.swing.JSpinner();
      jLabelInterval = new javax.swing.JLabel();
      jPanelSetConfig = new javax.swing.JPanel();
      jButtonSetConfig = new javax.swing.JButton();
      jPanelFilter = new javax.swing.JPanel();
      jButtonSetFilter = new javax.swing.JButton();
      jPanelUseNames = new javax.swing.JPanel();
      jCheckBoxUseNames = new javax.swing.JCheckBox();
      jPanelStatistics = new javax.swing.JPanel();
      jScrollPaneStatistics = new javax.swing.JScrollPane();
      jTableStatistics = new javax.swing.JTable();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setTitle(getFrameTitle());
      setDoubleBuffered(true);
      setPreferredSize(new java.awt.Dimension(950, 500));
      addInternalFrameListener(new javax.swing.event.InternalFrameListener()
      {
         public void internalFrameOpened(javax.swing.event.InternalFrameEvent evt)
         {
            formInternalFrameOpened(evt);
         }
         public void internalFrameClosing(javax.swing.event.InternalFrameEvent evt)
         {
         }
         public void internalFrameClosed(javax.swing.event.InternalFrameEvent evt)
         {
            formInternalFrameClosed(evt);
         }
         public void internalFrameIconified(javax.swing.event.InternalFrameEvent evt)
         {
         }
         public void internalFrameDeiconified(javax.swing.event.InternalFrameEvent evt)
         {
         }
         public void internalFrameActivated(javax.swing.event.InternalFrameEvent evt)
         {
         }
         public void internalFrameDeactivated(javax.swing.event.InternalFrameEvent evt)
         {
         }
      });

      jScrollPaneWindow.setPreferredSize(new java.awt.Dimension(650, 450));

      jPanelWindow.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelWindow.setPreferredSize(new java.awt.Dimension(900, 400));
      jPanelWindow.setLayout(new java.awt.BorderLayout());

      jPanelSettings.setPreferredSize(new java.awt.Dimension(300, 70));
      jPanelSettings.setLayout(new java.awt.FlowLayout(java.awt.FlowLayout.CENTER, 0, 5));

      jPanelConfig.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Settings")));
      jPanelConfig.setPreferredSize(new java.awt.Dimension(595, 60));

      jPanelLineCount.setPreferredSize(new java.awt.Dimension(231, 32));

      jSpinnerLineCount.setModel(new javax.swing.SpinnerNumberModel(DEFAULT_LINE_COUNT, MIN_LINE_COUNT, null, INCREMENT_LINE_COUNT));
      jSpinnerLineCount.setEditor(new javax.swing.JSpinner.NumberEditor(jSpinnerLineCount, ""));
      jSpinnerLineCount.setMinimumSize(new java.awt.Dimension(100, 28));
      jSpinnerLineCount.setPreferredSize(new java.awt.Dimension(100, 28));
      jSpinnerLineCount.setRequestFocusEnabled(false);
      jSpinnerLineCount.setValue(DEFAULT_LINE_COUNT);
      jSpinnerLineCount.addChangeListener(new javax.swing.event.ChangeListener()
      {
         public void stateChanged(javax.swing.event.ChangeEvent evt)
         {
            jSpinnerLineCountStateChanged(evt);
         }
      });

      jLabelLineCount.setText(Main.getLocal().getString("Number of clients:"));

      javax.swing.GroupLayout jPanelLineCountLayout = new javax.swing.GroupLayout(jPanelLineCount);
      jPanelLineCount.setLayout(jPanelLineCountLayout);
      jPanelLineCountLayout.setHorizontalGroup(
         jPanelLineCountLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, jPanelLineCountLayout.createSequentialGroup()
            .addContainerGap(15, Short.MAX_VALUE)
            .addComponent(jLabelLineCount)
            .addGap(6, 6, 6)
            .addComponent(jSpinnerLineCount, javax.swing.GroupLayout.PREFERRED_SIZE, 63, javax.swing.GroupLayout.PREFERRED_SIZE))
      );
      jPanelLineCountLayout.setVerticalGroup(
         jPanelLineCountLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelLineCountLayout.createSequentialGroup()
            .addGroup(jPanelLineCountLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
               .addComponent(jSpinnerLineCount, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
               .addComponent(jLabelLineCount))
            .addGap(21, 21, 21))
      );

      jPanelInterval.setPreferredSize(new java.awt.Dimension(200, 32));

      jSpinnerInterval.setModel(new javax.swing.SpinnerNumberModel(DEFAULT_INTERVAL, MIN_INTERVAL, null, INCREMENT_INTERVAL));
      jSpinnerInterval.setEditor(new javax.swing.JSpinner.NumberEditor(jSpinnerInterval, ""));
      jSpinnerInterval.setMinimumSize(new java.awt.Dimension(100, 28));
      jSpinnerInterval.setPreferredSize(new java.awt.Dimension(100, 28));
      jSpinnerInterval.setRequestFocusEnabled(false);
      jSpinnerInterval.setValue(DEFAULT_INTERVAL);
      ((JSpinner.DefaultEditor)jSpinnerInterval.getEditor()).getTextField().setBackground(Color.WHITE);
      jSpinnerInterval.addChangeListener(new javax.swing.event.ChangeListener()
      {
         public void stateChanged(javax.swing.event.ChangeEvent evt)
         {
            jSpinnerIntervalStateChanged(evt);
         }
      });

      jLabelInterval.setText(Main.getLocal().getString("Interval in sec:"));

      javax.swing.GroupLayout jPanelIntervalLayout = new javax.swing.GroupLayout(jPanelInterval);
      jPanelInterval.setLayout(jPanelIntervalLayout);
      jPanelIntervalLayout.setHorizontalGroup(
         jPanelIntervalLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelIntervalLayout.createSequentialGroup()
            .addContainerGap(14, Short.MAX_VALUE)
            .addComponent(jLabelInterval)
            .addGap(10, 10, 10)
            .addComponent(jSpinnerInterval, javax.swing.GroupLayout.PREFERRED_SIZE, 60, javax.swing.GroupLayout.PREFERRED_SIZE))
      );
      jPanelIntervalLayout.setVerticalGroup(
         jPanelIntervalLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelIntervalLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
            .addComponent(jSpinnerInterval, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addComponent(jLabelInterval))
      );

      jPanelSetConfig.setPreferredSize(new java.awt.Dimension(141, 32));

      jButtonSetConfig.setText(Main.getLocal().getString("Apply config"));
      jButtonSetConfig.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonSetConfigActionPerformed(evt);
         }
      });

      javax.swing.GroupLayout jPanelSetConfigLayout = new javax.swing.GroupLayout(jPanelSetConfig);
      jPanelSetConfig.setLayout(jPanelSetConfigLayout);
      jPanelSetConfigLayout.setHorizontalGroup(
         jPanelSetConfigLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, jPanelSetConfigLayout.createSequentialGroup()
            .addContainerGap(javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
            .addComponent(jButtonSetConfig)
            .addContainerGap())
      );
      jPanelSetConfigLayout.setVerticalGroup(
         jPanelSetConfigLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jButtonSetConfig, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
      );

      javax.swing.GroupLayout jPanelConfigLayout = new javax.swing.GroupLayout(jPanelConfig);
      jPanelConfig.setLayout(jPanelConfigLayout);
      jPanelConfigLayout.setHorizontalGroup(
         jPanelConfigLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, jPanelConfigLayout.createSequentialGroup()
            .addGap(6, 6, 6)
            .addComponent(jPanelInterval, javax.swing.GroupLayout.PREFERRED_SIZE, 188, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addGap(18, 18, 18)
            .addComponent(jPanelLineCount, javax.swing.GroupLayout.PREFERRED_SIZE, 213, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addGap(18, 18, 18)
            .addComponent(jPanelSetConfig, javax.swing.GroupLayout.PREFERRED_SIZE, 122, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addContainerGap(18, Short.MAX_VALUE))
      );
      jPanelConfigLayout.setVerticalGroup(
         jPanelConfigLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelConfigLayout.createSequentialGroup()
            .addGroup(jPanelConfigLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.TRAILING, false)
               .addComponent(jPanelLineCount, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.PREFERRED_SIZE, 28, Short.MAX_VALUE)
               .addComponent(jPanelInterval, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.DEFAULT_SIZE, 28, Short.MAX_VALUE))
            .addGap(0, 0, Short.MAX_VALUE))
         .addGroup(jPanelConfigLayout.createSequentialGroup()
            .addComponent(jPanelSetConfig, javax.swing.GroupLayout.DEFAULT_SIZE, 26, Short.MAX_VALUE)
            .addContainerGap())
      );

      jPanelSettings.add(jPanelConfig);

      jPanelFilter.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Filter")));
      jPanelFilter.setPreferredSize(new java.awt.Dimension(137, 60));

      jButtonSetFilter.setText(Main.getLocal().getString("Set Filter ..."));
      jButtonSetFilter.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonSetFilterActionPerformed(evt);
         }
      });

      javax.swing.GroupLayout jPanelFilterLayout = new javax.swing.GroupLayout(jPanelFilter);
      jPanelFilter.setLayout(jPanelFilterLayout);
      jPanelFilterLayout.setHorizontalGroup(
         jPanelFilterLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelFilterLayout.createSequentialGroup()
            .addContainerGap()
            .addComponent(jButtonSetFilter)
            .addContainerGap(javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE))
      );
      jPanelFilterLayout.setVerticalGroup(
         jPanelFilterLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelFilterLayout.createSequentialGroup()
            .addComponent(jButtonSetFilter)
            .addGap(0, 7, Short.MAX_VALUE))
      );

      jPanelSettings.add(jPanelFilter);

      jPanelUseNames.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Use Hostname")));
      jPanelUseNames.setPreferredSize(new java.awt.Dimension(120, 60));

      jCheckBoxUseNames.setToolTipText(Main.getLocal().getString("Use DNS resolution from the GUI host."));
      jCheckBoxUseNames.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
      jCheckBoxUseNames.addItemListener(new java.awt.event.ItemListener()
      {
         public void itemStateChanged(java.awt.event.ItemEvent evt)
         {
            jCheckBoxUseNamesItemStateChanged(evt);
         }
      });

      javax.swing.GroupLayout jPanelUseNamesLayout = new javax.swing.GroupLayout(jPanelUseNames);
      jPanelUseNames.setLayout(jPanelUseNamesLayout);
      jPanelUseNamesLayout.setHorizontalGroup(
         jPanelUseNamesLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jCheckBoxUseNames, javax.swing.GroupLayout.Alignment.TRAILING, javax.swing.GroupLayout.DEFAULT_SIZE, 108, Short.MAX_VALUE)
      );
      jPanelUseNamesLayout.setVerticalGroup(
         jPanelUseNamesLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelUseNamesLayout.createSequentialGroup()
            .addComponent(jCheckBoxUseNames)
            .addGap(0, 13, Short.MAX_VALUE))
      );

      jPanelSettings.add(jPanelUseNames);

      jPanelWindow.add(jPanelSettings, java.awt.BorderLayout.NORTH);

      jPanelStatistics.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Client Statistics")));
      jPanelStatistics.setPreferredSize(new java.awt.Dimension(900, 400));
      jPanelStatistics.setLayout(new java.awt.BorderLayout());

      jScrollPaneStatistics.setHorizontalScrollBarPolicy(javax.swing.ScrollPaneConstants.HORIZONTAL_SCROLLBAR_NEVER);
      jScrollPaneStatistics.setVerticalScrollBarPolicy(javax.swing.ScrollPaneConstants.VERTICAL_SCROLLBAR_NEVER);

      jTableStatistics.setModel(new StatsTableModel());
      jScrollPaneStatistics.setViewportView(jTableStatistics);

      jPanelStatistics.add(jScrollPaneStatistics, java.awt.BorderLayout.CENTER);

      jPanelWindow.add(jPanelStatistics, java.awt.BorderLayout.CENTER);

      jScrollPaneWindow.setViewportView(jPanelWindow);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneWindow, javax.swing.GroupLayout.DEFAULT_SIZE, 1039, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneWindow, javax.swing.GroupLayout.DEFAULT_SIZE, 470, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

   private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt)//GEN-FIRST:event_formInternalFrameClosed
   {//GEN-HEADEREND:event_formInternalFrameClosed
      this.setVisible(false);
      this.stopUpdateThread();
      FrameManager.delFrame(this);
   }//GEN-LAST:event_formInternalFrameClosed

   private void formInternalFrameOpened(javax.swing.event.InternalFrameEvent evt)//GEN-FIRST:event_formInternalFrameOpened
   {//GEN-HEADEREND:event_formInternalFrameOpened
      this.startUpdateThread();
   }//GEN-LAST:event_formInternalFrameOpened

   private void jButtonSetFilterActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonSetFilterActionPerformed
   {//GEN-HEADEREND:event_jButtonSetFilterActionPerformed
      JFrameStatsFilter filterDialog = new JFrameStatsFilter(this);
      filterDialog.setVisible(true);
   }//GEN-LAST:event_jButtonSetFilterActionPerformed

   private void jSpinnerIntervalStateChanged(javax.swing.event.ChangeEvent evt)//GEN-FIRST:event_jSpinnerIntervalStateChanged
   {//GEN-HEADEREND:event_jSpinnerIntervalStateChanged
      this.jButtonSetConfig.setEnabled(true);
   }//GEN-LAST:event_jSpinnerIntervalStateChanged

   private void jSpinnerLineCountStateChanged(javax.swing.event.ChangeEvent evt)//GEN-FIRST:event_jSpinnerLineCountStateChanged
   {//GEN-HEADEREND:event_jSpinnerLineCountStateChanged
      this.jButtonSetConfig.setEnabled(true);
   }//GEN-LAST:event_jSpinnerLineCountStateChanged

   private void jButtonSetConfigActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonSetConfigActionPerformed
   {//GEN-HEADEREND:event_jButtonSetConfigActionPerformed
      this.updateInterval = jSpinnerInterval.getValue().toString();
      this.clientCount = jSpinnerLineCount.getValue().toString();
      this.startUpdateThread();
      this.jButtonSetConfig.setEnabled(false);
   }//GEN-LAST:event_jButtonSetConfigActionPerformed

   private void jCheckBoxUseNamesItemStateChanged(java.awt.event.ItemEvent evt)//GEN-FIRST:event_jCheckBoxUseNamesItemStateChanged
   {//GEN-HEADEREND:event_jCheckBoxUseNamesItemStateChanged
      ((AbstractTableModel)jTableStatistics.getModel()).fireTableDataChanged();
   }//GEN-LAST:event_jCheckBoxUseNamesItemStateChanged


   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JButton jButtonSetConfig;
   private javax.swing.JButton jButtonSetFilter;
   private javax.swing.JCheckBox jCheckBoxUseNames;
   private javax.swing.JLabel jLabelInterval;
   private javax.swing.JLabel jLabelLineCount;
   private javax.swing.JPanel jPanelConfig;
   private javax.swing.JPanel jPanelFilter;
   private javax.swing.JPanel jPanelInterval;
   private javax.swing.JPanel jPanelLineCount;
   private javax.swing.JPanel jPanelSetConfig;
   private javax.swing.JPanel jPanelSettings;
   private javax.swing.JPanel jPanelStatistics;
   private javax.swing.JPanel jPanelUseNames;
   private javax.swing.JPanel jPanelWindow;
   private javax.swing.JScrollPane jScrollPaneStatistics;
   private javax.swing.JScrollPane jScrollPaneWindow;
   private javax.swing.JSpinner jSpinnerInterval;
   private javax.swing.JSpinner jSpinnerLineCount;
   private javax.swing.JTable jTableStatistics;
   // End of variables declaration//GEN-END:variables
}

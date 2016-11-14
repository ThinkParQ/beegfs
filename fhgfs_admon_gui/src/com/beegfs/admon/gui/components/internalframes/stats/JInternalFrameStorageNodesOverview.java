package com.beegfs.admon.gui.components.internalframes.stats;

import com.beegfs.admon.gui.common.ValueUnit;
import com.beegfs.admon.gui.common.enums.ColorLineTraceEnum;
import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import com.beegfs.admon.gui.common.enums.TimeUnitEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.UpdateThread;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.common.tools.UnitTk;
import com.beegfs.admon.gui.components.charts.ChartPoint;
import com.beegfs.admon.gui.components.charts.LineChart;
import com.beegfs.admon.gui.components.charts.LineTrace;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import java.util.ArrayList;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.DefaultComboBoxModel;
import javax.swing.ImageIcon;

public class JInternalFrameStorageNodesOverview extends javax.swing.JInternalFrame implements
        JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameStorageNodesOverview.class.getCanonicalName());
   private static final long serialVersionUID = 1L;

   private static final int DEFAULT_TRACE_COUNT = 4; // read, write, read-avg, write-avg

   private final String group;

   private transient StorageNodesOverviewUpdateThread updateThread;
   
   private LineChart chart;

   private JInternalFrameNodeStatus statusDetails = null;

   /**
    * Creates new form JInternalStorageNodesOverview
    */
   public JInternalFrameStorageNodesOverview()
   {
      this.group = "";
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
   }

   public JInternalFrameStorageNodesOverview(String group)
   {
      this.group = group;
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
   }

   public final String getGroup()
   {
      return this.group;
   }

   private void startUpdateThread()
   {
      updateThread = new StorageNodesOverviewUpdateThread(HttpTk.generateAdmonUrl(
         "/XML_StoragenodesOverview?group=") + group);
      updateThread.start();
   }

   private void stopUpdateThread()
   {
      this.updateThread.shouldStop();
   }



   private class StorageNodesOverviewUpdateThread extends UpdateThread
   {
      private static final String THREAD_NAME = "StorageOverview";

      StorageNodesOverviewUpdateThread(String parserUri)
      {
         super(parserUri, THREAD_NAME);
      }

      @Override
      public void run()
      {
         while (!stop)
         {
            try
            {
               lock.lock();
               try
               {
                  if(!gotData.await(10, TimeUnit.SECONDS))
                  {
                     LOGGER.log(Level.FINEST, "No update from server!");
                  }
               }
               finally
               {
                  lock.unlock();
               }

               int okCount = 0;
               int failCount = 0;

               ArrayList<TreeMap<String, String>> statusVals =
                       parser.getVectorOfAttributeTreeMaps("status");
               for (TreeMap<String, String> statusVal : statusVals)
               {
                  if (statusVal.get("value").equals("true"))
                  {
                     okCount++;
                  }
                  else
                  {
                     failCount++;
                  }
               }
               jLabelStatusOKCount.setText(String.valueOf(okCount));
               jLabelStatusFailCount.setText(String.valueOf(failCount));

               if(statusDetails != null)
               {
                  statusDetails.updateStatus(statusVals);
               }


               String diskSpaceTotalStr = parser.getValue("diskSpaceTotal", "diskSpace");
               jLabelDiskSpace.setText(diskSpaceTotalStr);

               String diskSpaceUsedStr = parser.getValue("diskSpaceUsed", "diskSpace");
               jLabelUsedDiskSpace.setText(diskSpaceUsedStr);

               String diskSpaceFreeStr = parser.getValue("diskSpaceFree", "diskSpace");

               jLabelFreeDiskSpace.setText(diskSpaceFreeStr);
               jPanelGeneral.repaint();

               //the disk perf chart
               ArrayList<ChartPoint> readVals = parser.getChartPointArrayList("diskPerfRead");
               ArrayList<ChartPoint> averageReadVals = parser.getChartPointArrayList(
                       "diskPerfAverageRead");
               ArrayList<ChartPoint> writeVals = parser.getChartPointArrayList("diskPerfWrite");
               ArrayList<ChartPoint> averageWriteVals = parser.getChartPointArrayList(
                       "diskPerfAverageWrite");

               ArrayList<Integer> tracesLength = new ArrayList<>(DEFAULT_TRACE_COUNT);
               tracesLength.add(readVals.size());
               tracesLength.add(averageReadVals.size());
               tracesLength.add(writeVals.size());
               tracesLength.add(averageWriteVals.size());

               String timeSpanStr = jComboBoxTimeSpan.getSelectedItem().toString();
               ValueUnit<TimeUnitEnum> vu = UnitTk.strToValueUnitOfTime(timeSpanStr);
               long timeSpan = UnitTk.timeSpanToMinutes(vu);

               GuiTk.addTracePointsToChart(readVals, chart, "Read", timeSpan, false,
                        ColorLineTraceEnum.COLOR_READ.color(), tracesLength);
               GuiTk.addTracePointsToChart(averageReadVals, chart, "Average Read", timeSpan, true,
                        ColorLineTraceEnum.COLOR_AVERAGE_READ.color(), tracesLength);
               GuiTk.addTracePointsToChart(writeVals, chart, "Write", timeSpan, false,
                        ColorLineTraceEnum.COLOR_WRITE.color(), tracesLength);
               GuiTk.addTracePointsToChart(averageWriteVals, chart, "Average Write", timeSpan, true,
                        ColorLineTraceEnum.COLOR_AVERAGE_WRITE.color(), tracesLength);
            }
            catch (CommunicationException e)
            {
               LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
               {
                  e,
                  true
               });
            }
            catch (java.lang.NullPointerException | java.lang.InterruptedException npe)
            {
               LOGGER.log(Level.FINEST, "Internal error.", npe);
            }
         }
         parser.shouldStop();
      }
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      if (!(obj instanceof JInternalFrameStorageNodesOverview))
      {
         return false;
      }
      
      JInternalFrameStorageNodesOverview objCasted = (JInternalFrameStorageNodesOverview) obj;
      return objCasted.getGroup().equals(group);
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

      jScrollPaneFrame = new javax.swing.JScrollPane();
      jPanelFrame = new javax.swing.JPanel();
      jPanelDiskPerf = new javax.swing.JPanel();
      jPanelControll = new javax.swing.JPanel();
      jLabelTimeSpan = new javax.swing.JLabel();
      jComboBoxTimeSpan = new javax.swing.JComboBox<>();
      jPanelDiskPerfGraph = new javax.swing.JPanel();
      jPanelGeneral = new javax.swing.JPanel();
      jLabelStatus = new javax.swing.JLabel();
      jLabelStatusOKIcon = new javax.swing.JLabel();
      jLabelStatusOKCount = new javax.swing.JLabel();
      jLabelStatusFailIcon = new javax.swing.JLabel();
      jLabelStatusFailCount = new javax.swing.JLabel();
      jLabelDiskSpaceText = new javax.swing.JLabel();
      jLabelDiskSpace = new javax.swing.JLabel();
      jLabelUsedDiskSpaceText = new javax.swing.JLabel();
      jLabelUsedDiskSpace = new javax.swing.JLabel();
      jLabelFreeDiskSpaceText = new javax.swing.JLabel();
      jLabelFreeDiskSpace = new javax.swing.JLabel();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setDoubleBuffered(true);
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

      jScrollPaneFrame.setDoubleBuffered(true);

      jPanelFrame.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelFrame.setPreferredSize(new java.awt.Dimension(814, 601));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jPanelDiskPerf.setBorder(javax.swing.BorderFactory.createTitledBorder("Throughput"));
      jPanelDiskPerf.setLayout(new java.awt.BorderLayout());

      jPanelControll.setLayout(new java.awt.FlowLayout(java.awt.FlowLayout.LEFT));

      jLabelTimeSpan.setText("Show the last ");
      jPanelControll.add(jLabelTimeSpan);

      jComboBoxTimeSpan.setModel(new DefaultComboBoxModel<>(com.beegfs.admon.gui.common.tools.DefinesTk.CHART_TIME_INTERVAL.toArray(new String[1])));
      jComboBoxTimeSpan.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jComboBoxTimeSpanActionPerformed(evt);
         }
      });
      jComboBoxTimeSpan.addPropertyChangeListener(new java.beans.PropertyChangeListener()
      {
         public void propertyChange(java.beans.PropertyChangeEvent evt)
         {
            jComboBoxTimeSpanPropertyChange(evt);
         }
      });
      jPanelControll.add(jComboBoxTimeSpan);

      jPanelDiskPerf.add(jPanelControll, java.awt.BorderLayout.NORTH);

      jPanelDiskPerfGraph.setLayout(new java.awt.GridLayout(1, 0));
      jPanelDiskPerf.add(jPanelDiskPerfGraph, java.awt.BorderLayout.CENTER);
      ArrayList<ChartPoint> readVals = new ArrayList<>(0);
      ArrayList<ChartPoint> averageReadVals = new ArrayList<>(0);
      ArrayList<ChartPoint> writeVals = new ArrayList<>(0);
      ArrayList<ChartPoint> averageWriteVals = new ArrayList<>(0);

      LineTrace readTrace = new LineTrace("Read", ColorLineTraceEnum.COLOR_READ.color(), readVals);
      LineTrace writeTrace = new LineTrace("Write", ColorLineTraceEnum.COLOR_WRITE.color(),
         writeVals);
      LineTrace averageReadTrace = new LineTrace("Average Read",
         ColorLineTraceEnum.COLOR_AVERAGE_READ.color(), averageReadVals);
      LineTrace averageWriteTrace = new LineTrace("Average Write",
         ColorLineTraceEnum.COLOR_AVERAGE_WRITE.color(), averageWriteVals);

      LineTrace[] traces = new LineTrace[DEFAULT_TRACE_COUNT];
      traces[0] = readTrace;
      traces[1] = writeTrace;
      traces[2] = averageReadTrace;
      traces[3] = averageWriteTrace;

      chart = new LineChart("Throughput (MiB/s)", "", "", 600, traces);

      jPanelDiskPerfGraph.add(chart);

      jPanelFrame.add(jPanelDiskPerf, java.awt.BorderLayout.CENTER);

      jPanelGeneral.setBorder(javax.swing.BorderFactory.createTitledBorder("General information"));
      jPanelGeneral.setMinimumSize(new java.awt.Dimension(830, 40));
      jPanelGeneral.setPreferredSize(new java.awt.Dimension(818, 40));
      jPanelGeneral.setLayout(new java.awt.GridBagLayout());

      jLabelStatus.setText("Status:");
      jLabelStatus.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jLabelStatusMouseClicked(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelStatus, gridBagConstraints);

      jLabelStatusOKIcon.setIcon(new ImageIcon(JInternalFrameStorageNodesOverview.class.getResource(FilePathsEnum.IMAGE_STATUS_OK.getPath())));
      jLabelStatusOKIcon.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jLabelStatusOKIconMouseClicked(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelStatusOKIcon, gridBagConstraints);

      jLabelStatusOKCount.setForeground(java.awt.Color.blue);
      jLabelStatusOKCount.setText("0");
      jLabelStatusOKCount.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jLabelStatusOKCountMouseClicked(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelStatusOKCount, gridBagConstraints);

      jLabelStatusFailIcon.setIcon(new ImageIcon(JInternalFrameStorageNodesOverview.class.getResource(FilePathsEnum.IMAGE_STATUS_FAIL.getPath())));
      jLabelStatusFailIcon.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jLabelStatusFailIconMouseClicked(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 3;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelStatusFailIcon, gridBagConstraints);

      jLabelStatusFailCount.setForeground(java.awt.Color.blue);
      jLabelStatusFailCount.setText("0");
      jLabelStatusFailCount.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jLabelStatusFailCountMouseClicked(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 4;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 30);
      jPanelGeneral.add(jLabelStatusFailCount, gridBagConstraints);

      jLabelDiskSpaceText.setText("Disk space :");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 5;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 30, 5, 5);
      jPanelGeneral.add(jLabelDiskSpaceText, gridBagConstraints);

      jLabelDiskSpace.setText("0");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 6;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelDiskSpace, gridBagConstraints);

      jLabelUsedDiskSpaceText.setText("Used disk space :");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 7;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelUsedDiskSpaceText, gridBagConstraints);

      jLabelUsedDiskSpace.setText("0");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 8;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelUsedDiskSpace, gridBagConstraints);

      jLabelFreeDiskSpaceText.setText("Free disk space :");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 9;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.ipadx = 7;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelFreeDiskSpaceText, gridBagConstraints);

      jLabelFreeDiskSpace.setText("0");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 10;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelFreeDiskSpace, gridBagConstraints);

      jPanelFrame.add(jPanelGeneral, java.awt.BorderLayout.NORTH);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 820, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 637, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      stopUpdateThread();
      if(statusDetails != null)
      {
         statusDetails.dispose();
      }
      FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

    private void jComboBoxTimeSpanActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jComboBoxTimeSpanActionPerformed
       String timeSpanStr = jComboBoxTimeSpan.getSelectedItem().toString();
       ValueUnit<TimeUnitEnum> vu = UnitTk.strToValueUnitOfTime(timeSpanStr);

       try
       {
          updateThread.getParser().setUrl(HttpTk.generateAdmonUrl(
                  "/XML_StoragenodesOverview?timeSpanPerf=" + String.valueOf(
                  UnitTk.timeSpanToMinutes(vu))));
          if (UnitTk.timeSpanToMinutes(vu) > 1440)
          {
             this.chart.setFullDateFormat(true);
          }
          else
          {
             this.chart.setFullDateFormat(false);
          }
          updateThread.getParser().update();

       }
       catch (ArrayIndexOutOfBoundsException e)
       {
          LOGGER.log(Level.FINEST, "Internal error.", e);
       }
}//GEN-LAST:event_jComboBoxTimeSpanActionPerformed

    private void jComboBoxTimeSpanPropertyChange(java.beans.PropertyChangeEvent evt) {//GEN-FIRST:event_jComboBoxTimeSpanPropertyChange
}//GEN-LAST:event_jComboBoxTimeSpanPropertyChange

   private void formInternalFrameOpened(javax.swing.event.InternalFrameEvent evt)//GEN-FIRST:event_formInternalFrameOpened
   {//GEN-HEADEREND:event_formInternalFrameOpened
      startUpdateThread();
   }//GEN-LAST:event_formInternalFrameOpened

   private void jLabelStatusFailCountMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusFailCountMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusFailCountMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusFailCountMouseClicked

   private void jLabelStatusOKCountMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusOKCountMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusOKCountMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusOKCountMouseClicked

   private void jLabelStatusOKIconMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusOKIconMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusOKIconMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusOKIconMouseClicked

   private void jLabelStatusFailIconMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusFailIconMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusFailIconMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusFailIconMouseClicked

   private void jLabelStatusMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusMouseClicked

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JComboBox<String> jComboBoxTimeSpan;
   private javax.swing.JLabel jLabelDiskSpace;
   private javax.swing.JLabel jLabelDiskSpaceText;
   private javax.swing.JLabel jLabelFreeDiskSpace;
   private javax.swing.JLabel jLabelFreeDiskSpaceText;
   private javax.swing.JLabel jLabelStatus;
   private javax.swing.JLabel jLabelStatusFailCount;
   private javax.swing.JLabel jLabelStatusFailIcon;
   private javax.swing.JLabel jLabelStatusOKCount;
   private javax.swing.JLabel jLabelStatusOKIcon;
   private javax.swing.JLabel jLabelTimeSpan;
   private javax.swing.JLabel jLabelUsedDiskSpace;
   private javax.swing.JLabel jLabelUsedDiskSpaceText;
   private javax.swing.JPanel jPanelControll;
   private javax.swing.JPanel jPanelDiskPerf;
   private javax.swing.JPanel jPanelDiskPerfGraph;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelGeneral;
   private javax.swing.JScrollPane jScrollPaneFrame;
   // End of variables declaration//GEN-END:variables

   @Override
   public final String getFrameTitle()
   {
      return "Storage Nodes Overview";
   }

   private void showStatusDetails()
   {
      ArrayList<TreeMap<String, String>> statusVals;
      
      try
      {
         statusVals = this.updateThread.getParser().getVectorOfAttributeTreeMaps("status");

         JInternalFrameNodeStatus frame = new JInternalFrameNodeStatus(NodeTypesEnum.STORAGE,
            statusVals);
         
         if (!FrameManager.isFrameOpen(frame, true))
         {
            this.getDesktopPane().add(frame);
            frame.setVisible(true);
            FrameManager.addFrame(frame);

            this.statusDetails = frame;
         }
      }
      catch (CommunicationException ex)
      {
         LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
         {
            ex,
            true
         });
      }
   }
}
package com.beegfs.admon.gui.components.internalframes.stats;

import com.beegfs.admon.gui.common.ValueUnit;
import com.beegfs.admon.gui.common.enums.ColorLineTraceEnum;
import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.common.enums.SizeUnitEnum;
import com.beegfs.admon.gui.common.enums.TimeUnitEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.nodes.Node;
import com.beegfs.admon.gui.common.threading.UpdateThread;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.common.tools.UnitTk;
import com.beegfs.admon.gui.components.charts.ChartPoint;
import com.beegfs.admon.gui.components.charts.LineChart;
import com.beegfs.admon.gui.components.charts.LineTrace;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.components.tables.CenteredTableCellRenderer;
import com.beegfs.admon.gui.program.Main;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.DefaultComboBoxModel;
import javax.swing.ImageIcon;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

public class JInternalFrameStorageNode extends javax.swing.JInternalFrame implements
        JInternalFrameInterface
{
   private static final Logger LOGGER = Logger.getLogger(
      JInternalFrameStorageNode.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "StorageNode";

   private static final int DEFAULT_TRACE_COUNT = 4; // read, write, read-avg, write-avg

   private final transient Node node;

   private LineChart chart;

   private transient StorageNodeUpdateThread updateThread;

   /**
    * Creates new form JInternalFrameMetaNode
    * @param node The node object of the node to show the storage statistics.
    */
   public JInternalFrameStorageNode(Node node)
   {
      this.node = node;
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
   }

   private void startUpdateThread()
   {
      String parserUri = HttpTk.generateAdmonUrl("/XML_Storagenode?node=") + node.getNodeID() +
         "&nodeNumID=" + String.valueOf(node.getNodeNumID() );
      updateThread = new StorageNodeUpdateThread(parserUri, THREAD_NAME + node.getNodeID());
      updateThread.start();
   }

   private void stopUpdateThread()
   {
      updateThread.shouldStop();
   }

   private class StorageNodeUpdateThread extends UpdateThread
   {
      StorageNodeUpdateThread(String parserUrl, String name)
      {
         super(parserUrl, name);
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

               TreeMap<String, String> data = parser.getTreeMap("general");
               if (!data.isEmpty())
               {
                  jLabelNodeID.setText(data.get("nodeID") + " [ID: " + data.get("nodeNumID") + "]");

                  if (data.get("status").equals("true"))
                  {
                     jLabelStatus.setIcon(new ImageIcon(getClass().getResource(
                           FilePathsEnum.IMAGE_STATUS_OK.getPath()))); // NOI18N
                  }
                  else
                  {
                     jLabelStatus.setIcon(new ImageIcon(getClass().getResource(
                           FilePathsEnum.IMAGE_STATUS_FAIL.getPath()))); // NOI18N
                  }

                  jPanelGeneral.repaint();
               }

               // storage targets
               ArrayList<TreeMap<String, String>> entries = parser.getVectorOfAttributeTreeMaps(
                       "storageTargets");
               Iterator<TreeMap<String, String>> iter = entries.iterator();
               DefaultTableModel model = (DefaultTableModel) jTableTargets.getModel();
               TableCellRenderer cellRenderer = new CenteredTableCellRenderer();

               for (int i = 0; i < jTableTargets.getColumnCount(); i++)
               {
                  TableColumn col = jTableTargets.getColumnModel().getColumn(i);
                  col.setCellRenderer(cellRenderer);
               }

               for (int i = model.getRowCount() - 1; i >= 0; i--)
               {
                  model.removeRow(i);
               }

               while (iter.hasNext())
               {
                  try
                  {
                     TreeMap<String, String> entry = iter.next();
                     String targetID = entry.get("value");
                     String targetPath = entry.get("pathStr");

                     SizeUnitEnum unit = SizeUnitEnum.BYTE;
                     double value = Double.parseDouble(entry.get("diskSpaceTotal"));
                     ValueUnit<SizeUnitEnum> diskSpaceTotal = new ValueUnit<>(value, unit);

                     value = Double.parseDouble(entry.get("diskSpaceFree"));
                     ValueUnit<SizeUnitEnum> diskSpaceFree = new ValueUnit<>(value, unit);

                     ValueUnit<SizeUnitEnum> diskSpaceUsed =
                        UnitTk.subtract(diskSpaceTotal, diskSpaceFree);

                     Object[] row = new Object[]
                     {
                        targetID,
                        targetPath,
                        UnitTk.xbyteToXbyte(diskSpaceTotal).toString(),
                        UnitTk.xbyteToXbyte(diskSpaceUsed).toString(),
                        UnitTk.xbyteToXbyte(diskSpaceFree).toString()
                     };

                     model.addRow(row);
                  }
                  catch (NumberFormatException e)
                  {
                     LOGGER.log(Level.FINEST, "Internal error.", e);
                  }
               }

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

               GuiTk.addTracePointsToChart(readVals, chart, Main.getLocal().getString("Read"), timeSpan, false,
                        ColorLineTraceEnum.COLOR_READ.color(), tracesLength);
               GuiTk.addTracePointsToChart(averageReadVals, chart, Main.getLocal().getString("Average Read"), timeSpan, true,
                        ColorLineTraceEnum.COLOR_AVERAGE_READ.color(), tracesLength);
               GuiTk.addTracePointsToChart(writeVals, chart, Main.getLocal().getString("Write"), timeSpan, false,
                        ColorLineTraceEnum.COLOR_WRITE.color(), tracesLength);
               GuiTk.addTracePointsToChart(averageWriteVals, chart, Main.getLocal().getString("Average Write"), timeSpan, true,
                       ColorLineTraceEnum.COLOR_AVERAGE_WRITE.color(), tracesLength);
            }
            catch (CommunicationException e)
            {
               LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
               {
                  e,
                  true
               });
            }
            catch (java.lang.NullPointerException | java.lang.InterruptedException npe)
            {
               LOGGER.log(Level.FINEST, Main.getLocal().getString("Internal error."), npe);
            }
         }
         parser.shouldStop();
      }
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      if (!(obj instanceof JInternalFrameStorageNode))
      {
         return false;
      }

      JInternalFrameStorageNode objCasted = (JInternalFrameStorageNode) obj;

      return objCasted.getNode().getNodeNumID() == node.getNodeNumID();
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
      jSplitPanePerfStorage = new javax.swing.JSplitPane();
      jPanelDiskPerf = new javax.swing.JPanel();
      jPanelControll = new javax.swing.JPanel();
      jLabelTimeSpan = new javax.swing.JLabel();
      jComboBoxTimeSpan = new javax.swing.JComboBox<>();
      jPanelDiskPerfGraph = new javax.swing.JPanel();
      jPanelStorageTargets = new javax.swing.JPanel();
      jScrollPaneTargets = new javax.swing.JScrollPane();
      jTableTargets = new javax.swing.JTable();
      jPanelGeneral = new javax.swing.JPanel();
      jLabelNodeIDText = new javax.swing.JLabel();
      jLabelNodeID = new javax.swing.JLabel();
      jLabelStatusText = new javax.swing.JLabel();
      jLabelStatus = new javax.swing.JLabel();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
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

      jPanelFrame.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelFrame.setPreferredSize(new java.awt.Dimension(852, 450));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jSplitPanePerfStorage.setDividerLocation(250);
      jSplitPanePerfStorage.setOrientation(javax.swing.JSplitPane.VERTICAL_SPLIT);

      jPanelDiskPerf.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Throughput")));
      jPanelDiskPerf.setPreferredSize(new java.awt.Dimension(400, 250));
      jPanelDiskPerf.setLayout(new java.awt.BorderLayout());

      jPanelControll.setPreferredSize(new java.awt.Dimension(400, 40));
      jPanelControll.setRequestFocusEnabled(false);
      jPanelControll.setLayout(new java.awt.FlowLayout(java.awt.FlowLayout.LEFT));

      jLabelTimeSpan.setText(Main.getLocal().getString("Show the last "));
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

      jPanelDiskPerfGraph.setPreferredSize(new java.awt.Dimension(400, 200));
      jPanelDiskPerfGraph.setLayout(new java.awt.GridLayout(1, 0));

      ArrayList<ChartPoint> readVals = new ArrayList<>(0);
      ArrayList<ChartPoint> averageReadVals = new ArrayList<>(0);
      ArrayList<ChartPoint> writeVals = new ArrayList<>(0);
      ArrayList<ChartPoint> averageWriteVals = new ArrayList<>(0);
      LineTrace readTrace = new LineTrace(Main.getLocal().getString("Read"),
         ColorLineTraceEnum.COLOR_READ.color(), readVals);
      LineTrace writeTrace = new LineTrace(Main.getLocal().getString("Write"),
         ColorLineTraceEnum.COLOR_WRITE.color(), writeVals);
      LineTrace averageReadTrace = new LineTrace(Main.getLocal().getString("Average Read"),
         ColorLineTraceEnum.COLOR_AVERAGE_READ.color(), averageReadVals);
      LineTrace averageWriteTrace = new LineTrace(Main.getLocal().getString("Average Write"),
         ColorLineTraceEnum.COLOR_AVERAGE_WRITE.color(), averageWriteVals);

      LineTrace[] traces = new LineTrace[DEFAULT_TRACE_COUNT];
      traces[0] = readTrace;
      traces[1] = writeTrace;
      traces[2] = averageReadTrace;
      traces[3] = averageWriteTrace;

      chart = new LineChart(Main.getLocal().getString("Throughput (MiB/s)"), "","", 600, traces);

      jPanelDiskPerfGraph.add(chart);

      jPanelDiskPerf.add(jPanelDiskPerfGraph, java.awt.BorderLayout.CENTER);

      jSplitPanePerfStorage.setLeftComponent(jPanelDiskPerf);

      jPanelStorageTargets.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Storage Targets")));
      jPanelStorageTargets.setMinimumSize(new java.awt.Dimension(0, 0));
      jPanelStorageTargets.setPreferredSize(new java.awt.Dimension(400, 200));
      jPanelStorageTargets.setLayout(new java.awt.BorderLayout());

      jScrollPaneTargets.setMinimumSize(new java.awt.Dimension(0, 0));
      jScrollPaneTargets.setPreferredSize(new java.awt.Dimension(400, 200));

      jTableTargets.setModel(new javax.swing.table.DefaultTableModel(
         new Object [][]
         {

         },
         new String []
         {
            Main.getLocal().getString("Target ID"), Main.getLocal().getString("Storage path"), Main.getLocal().getString("Total disk space"), Main.getLocal().getString("Used disk space"), Main.getLocal().getString("Free disk space")
         }
      ));
      jTableTargets.setMinimumSize(new java.awt.Dimension(0, 0));
      jTableTargets.setPreferredSize(new java.awt.Dimension(400, 200));
      jScrollPaneTargets.setViewportView(jTableTargets);

      jPanelStorageTargets.add(jScrollPaneTargets, java.awt.BorderLayout.CENTER);

      jSplitPanePerfStorage.setRightComponent(jPanelStorageTargets);

      jPanelFrame.add(jSplitPanePerfStorage, java.awt.BorderLayout.CENTER);

      jPanelGeneral.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("General Information")));
      jPanelGeneral.setLayout(new java.awt.GridBagLayout());

      jLabelNodeIDText.setHorizontalAlignment(javax.swing.SwingConstants.TRAILING);
      jLabelNodeIDText.setText(Main.getLocal().getString("Node ID : "));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelNodeIDText, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelNodeID, gridBagConstraints);

      jLabelStatusText.setHorizontalAlignment(javax.swing.SwingConstants.TRAILING);
      jLabelStatusText.setText(Main.getLocal().getString("Status : "));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelStatusText, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 3;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelStatus, gridBagConstraints);

      jPanelFrame.add(jPanelGeneral, java.awt.BorderLayout.NORTH);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 858, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 497, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      stopUpdateThread();
      FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

    private void jComboBoxTimeSpanActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jComboBoxTimeSpanActionPerformed
       String timeSpanStr = jComboBoxTimeSpan.getSelectedItem().toString();
       ValueUnit<TimeUnitEnum> vu = UnitTk.strToValueUnitOfTime(timeSpanStr);

       try
       {
          updateThread.getParser().setUrl(HttpTk.generateAdmonUrl("/XML_Storagenode?node=" +
                  node.getNodeID() + "&nodeNumID=" + String.valueOf(node.getNodeNumID()) +
                  "&timeSpanPerf=" + String.valueOf(UnitTk.timeSpanToMinutes(vu))));
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

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JComboBox<String> jComboBoxTimeSpan;
   private javax.swing.JLabel jLabelNodeID;
   private javax.swing.JLabel jLabelNodeIDText;
   private javax.swing.JLabel jLabelStatus;
   private javax.swing.JLabel jLabelStatusText;
   private javax.swing.JLabel jLabelTimeSpan;
   private javax.swing.JPanel jPanelControll;
   private javax.swing.JPanel jPanelDiskPerf;
   private javax.swing.JPanel jPanelDiskPerfGraph;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelGeneral;
   private javax.swing.JPanel jPanelStorageTargets;
   private javax.swing.JScrollPane jScrollPaneFrame;
   private javax.swing.JScrollPane jScrollPaneTargets;
   private javax.swing.JSplitPane jSplitPanePerfStorage;
   private javax.swing.JTable jTableTargets;
   // End of variables declaration//GEN-END:variables

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("Storage Node: ") + node.getTypedNodeID();
   }

   public Node getNode()
   {
      return this.node;
   }
}

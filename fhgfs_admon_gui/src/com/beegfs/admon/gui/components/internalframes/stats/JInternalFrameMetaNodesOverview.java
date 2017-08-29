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
import com.beegfs.admon.gui.program.Main;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.DefaultComboBoxModel;
import javax.swing.ImageIcon;
import java.text.MessageFormat;

/**
 *
 *
 */
public class JInternalFrameMetaNodesOverview extends javax.swing.JInternalFrame
        implements JInternalFrameInterface
{
   private static final Logger LOGGER = Logger.getLogger(
      JInternalFrameMetaNodesOverview.class.getCanonicalName());
   private static final long serialVersionUID = 1L;

   private static final int DEFAULT_TRACE_COUNT = 1;

   private transient MetaNodesOverviewUpdateThread updateThread;
   private final String group;
   private LineChart workRequestsChart;
   private LineChart queuedWorkRequestsChart;

   private JInternalFrameNodeStatus statusDetails = null;

   /**
    * Creates new form JInternalFrameMetaNodesOverview
    */
   public JInternalFrameMetaNodesOverview()
   {
      this.group = "";
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
   }

   public JInternalFrameMetaNodesOverview(String group)
   {
      this.group = group;
      initComponents();
      setTitle(MessageFormat.format(Main.getLocal().getString("Metadata nodes overview (Group : {0})"), group));
      setFrameIcon(GuiTk.getFrameIcon());
   }

   private void startUpdateThread()
   {
      updateThread = new MetaNodesOverviewUpdateThread("XML_MetanodesOverview");
      updateThread.start();
   }

   private void stopUpdateThread()
   {
      updateThread.shouldStop();
   }

   public final String getGroup()
   {
      return this.group;
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      if (!(obj instanceof JInternalFrameMetaNodesOverview))
      {
         return false;
      }

      JInternalFrameMetaNodesOverview objCasted = (JInternalFrameMetaNodesOverview) obj;
      return objCasted.getGroup().equals(group);
   }

   private class MetaNodesOverviewUpdateThread extends UpdateThread
   {
      private static final String THREAD_NAME = "MetaOverview";

      private static final int DEFAULT_PARAMETER_COUNT = 1;

      private final String xmlPage;
      private final HashMap<String, String> parameters;

      MetaNodesOverviewUpdateThread(String parserUri)
      {
         super(parserUri, THREAD_NAME);
         xmlPage = parserUri;
         parameters = new HashMap<>(DEFAULT_PARAMETER_COUNT);
         parameters.put("timeSpanRequests", "10");
         parser.setUrl(buildURL());
      }

      private String buildURL()
      {
         StringBuilder u = new StringBuilder(HttpTk.generateAdmonUrl("/" + xmlPage + "?") );
         Iterator<Map.Entry<String,String>> i = parameters.entrySet().iterator();
         int paramCount = 0;

         while (i.hasNext())
         {
            if (paramCount != 0)
            {
               u.append("&");
            }
            Map.Entry<String,String> pair = i.next();
            String key = pair.getKey();
            String value = pair.getValue();
            u.append(key);
            u.append("=");
            u.append(value);
         }
         return u.toString();
      }

      private void setAttribute(String key, String value)
      {
         parameters.put(key, value);
         String url = buildURL();
         parser.setUrl(url);
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
               TreeMap<String, String> generalVals = parser.getTreeMap("general");

               jLabelNodeCount.setText(generalVals.get("nodeCount"));
               jLabelRootNode.setText(generalVals.get("rootNode"));
               jPanelGeneral.revalidate();

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

               ArrayList<ChartPoint> workRequests = parser.getChartPointArrayList("workRequests");

               ArrayList<ChartPoint> queuedWorkRequests = parser.getChartPointArrayList(
                       "queuedRequests");

               String timeSpanStr = jComboBoxTimeSpan.getSelectedItem().toString();
               ValueUnit<TimeUnitEnum> vu = UnitTk.strToValueUnitOfTime(timeSpanStr);
               long timeSpan = UnitTk.timeSpanToMinutes(vu);

               GuiTk.addTracePointsToChart(workRequests, workRequestsChart, Main.getLocal().getString("Work Requests"),
                        timeSpan, false, ColorLineTraceEnum.COLOR_META_OPS.color(),
                        new ArrayList<Integer>(0));
               GuiTk.addTracePointsToChart(queuedWorkRequests, queuedWorkRequestsChart,
            		   Main.getLocal().getString("Queued Work Requests"), timeSpan, false,
                        ColorLineTraceEnum.COLOR_META_OPS.color(), new ArrayList<Integer>(0));

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
               LOGGER.log(Level.FINEST, "Internal error.", npe);
            }

         }
         parser.shouldStop();
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
      java.awt.GridBagConstraints gridBagConstraints;

      jScrollPaneFrame = new javax.swing.JScrollPane();
      jPanelFrame = new javax.swing.JPanel();
      jPanelGeneral = new javax.swing.JPanel();
      jLabelNodeCountText = new javax.swing.JLabel();
      jLabelNodeCount = new javax.swing.JLabel();
      jLabelRootNode = new javax.swing.JLabel();
      jLabelRodeNodeText = new javax.swing.JLabel();
      jLabelStatus = new javax.swing.JLabel();
      jLabelStatusOKIcon = new javax.swing.JLabel();
      jLabelStatusOKCount = new javax.swing.JLabel();
      jLabelStatusFailIcon = new javax.swing.JLabel();
      jLabelStatusFailCount = new javax.swing.JLabel();
      jPanelWorkRequests = new javax.swing.JPanel();
      jPanelRequests = new javax.swing.JPanel();
      jLabelTimeSpan = new javax.swing.JLabel();
      jComboBoxTimeSpan = new javax.swing.JComboBox<>();
      jSplitPane1 = new javax.swing.JSplitPane();
      jPanelWorkRequestsGraph = new javax.swing.JPanel();
      jPanelQueuedWorkRequestsGraph = new javax.swing.JPanel();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setTitle(Main.getLocal().getString("Metadata nodes overview"));
      setDoubleBuffered(true);
      setPreferredSize(new java.awt.Dimension(780, 686));
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
      jPanelFrame.setPreferredSize(new java.awt.Dimension(764, 648));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jPanelGeneral.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("General information")));
      jPanelGeneral.setLayout(new java.awt.GridBagLayout());

      jLabelNodeCountText.setText(Main.getLocal().getString("Number of metadata nodes : "));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 5;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 30, 5, 5);
      jPanelGeneral.add(jLabelNodeCountText, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 6;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelNodeCount, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 8;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelRootNode, gridBagConstraints);

      jLabelRodeNodeText.setText(Main.getLocal().getString("Root metadata node :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 7;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelGeneral.add(jLabelRodeNodeText, gridBagConstraints);

      jLabelStatus.setText(Main.getLocal().getString("Status:"));
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

      jLabelStatusOKIcon.setIcon(new ImageIcon(JInternalFrameMetaNodesOverview.class.getResource(FilePathsEnum.IMAGE_STATUS_OK.getPath())));
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

      jLabelStatusFailIcon.setIcon(new ImageIcon(JInternalFrameMetaNodesOverview.class.getResource(FilePathsEnum.IMAGE_STATUS_FAIL.getPath())));
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

      jPanelFrame.add(jPanelGeneral, java.awt.BorderLayout.NORTH);

      jPanelWorkRequests.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Work Requests")));
      jPanelWorkRequests.setLayout(new java.awt.BorderLayout(5, 5));

      jPanelRequests.setLayout(new java.awt.FlowLayout(java.awt.FlowLayout.LEFT));

      jLabelTimeSpan.setText(Main.getLocal().getString("Show the last "));
      jPanelRequests.add(jLabelTimeSpan);

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
      jPanelRequests.add(jComboBoxTimeSpan);

      jPanelWorkRequests.add(jPanelRequests, java.awt.BorderLayout.NORTH);

      jSplitPane1.setDividerLocation(250);
      jSplitPane1.setOrientation(javax.swing.JSplitPane.VERTICAL_SPLIT);

      jPanelWorkRequestsGraph.setPreferredSize(new java.awt.Dimension(300, 300));
      jPanelWorkRequestsGraph.setLayout(new java.awt.GridLayout(1, 0));

      ArrayList<ChartPoint> workRequests = new ArrayList<>(0);

      LineTrace workRequestsTrace = new LineTrace(Main.getLocal().getString("Work Requests"),
         ColorLineTraceEnum.COLOR_META_OPS.color(), workRequests);

      LineTrace[] workRequestTraces = new LineTrace[DEFAULT_TRACE_COUNT];
      workRequestTraces[0] = workRequestsTrace;

      workRequestsChart = new LineChart(Main.getLocal().getString("Work Requests"), "", "", 600, workRequestTraces, false,
         false);

      jPanelWorkRequestsGraph.add(workRequestsChart);

      jSplitPane1.setTopComponent(jPanelWorkRequestsGraph);

      jPanelQueuedWorkRequestsGraph.setPreferredSize(new java.awt.Dimension(300, 300));
      jPanelQueuedWorkRequestsGraph.setLayout(new java.awt.GridLayout(1, 0));

      ArrayList<ChartPoint> queuedWorkRequests = new ArrayList<>(0);

      LineTrace queuedWorkRequestsTrace = new LineTrace(Main.getLocal().getString("Queued Work Requests"),
         ColorLineTraceEnum.COLOR_META_OPS.color(), queuedWorkRequests);

      LineTrace[] queuedWorkRequestTraces = new LineTrace[DEFAULT_TRACE_COUNT];
      queuedWorkRequestTraces[0] = queuedWorkRequestsTrace;

      queuedWorkRequestsChart = new LineChart(Main.getLocal().getString("Queued Work Requests"), "", "", 600,
         queuedWorkRequestTraces, false, false);

      jPanelQueuedWorkRequestsGraph.add(queuedWorkRequestsChart);

      jSplitPane1.setBottomComponent(jPanelQueuedWorkRequestsGraph);

      jPanelWorkRequests.add(jSplitPane1, java.awt.BorderLayout.CENTER);

      jPanelFrame.add(jPanelWorkRequests, java.awt.BorderLayout.CENTER);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 769, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 656, Short.MAX_VALUE)
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
          updateThread.setAttribute("timeSpanRequests", String.valueOf(
                  UnitTk.timeSpanToMinutes(vu)));
          if (UnitTk.timeSpanToMinutes(vu) > 1440)
          {
             this.workRequestsChart.setFullDateFormat(true);
             this.queuedWorkRequestsChart.setFullDateFormat(true);
          }
          else
          {
             this.workRequestsChart.setFullDateFormat(false);
             this.queuedWorkRequestsChart.setFullDateFormat(false);
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

   private void jLabelStatusMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusMouseClicked

   private void jLabelStatusOKIconMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusOKIconMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusOKIconMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusOKIconMouseClicked

   private void jLabelStatusOKCountMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusOKCountMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusOKCountMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusOKCountMouseClicked

   private void jLabelStatusFailIconMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusFailIconMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusFailIconMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusFailIconMouseClicked

   private void jLabelStatusFailCountMouseClicked(java.awt.event.MouseEvent evt)//GEN-FIRST:event_jLabelStatusFailCountMouseClicked
   {//GEN-HEADEREND:event_jLabelStatusFailCountMouseClicked
      showStatusDetails();
   }//GEN-LAST:event_jLabelStatusFailCountMouseClicked

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JComboBox<String> jComboBoxTimeSpan;
   private javax.swing.JLabel jLabelNodeCount;
   private javax.swing.JLabel jLabelNodeCountText;
   private javax.swing.JLabel jLabelRodeNodeText;
   private javax.swing.JLabel jLabelRootNode;
   private javax.swing.JLabel jLabelStatus;
   private javax.swing.JLabel jLabelStatusFailCount;
   private javax.swing.JLabel jLabelStatusFailIcon;
   private javax.swing.JLabel jLabelStatusOKCount;
   private javax.swing.JLabel jLabelStatusOKIcon;
   private javax.swing.JLabel jLabelTimeSpan;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelGeneral;
   private javax.swing.JPanel jPanelQueuedWorkRequestsGraph;
   private javax.swing.JPanel jPanelRequests;
   private javax.swing.JPanel jPanelWorkRequests;
   private javax.swing.JPanel jPanelWorkRequestsGraph;
   private javax.swing.JScrollPane jScrollPaneFrame;
   private javax.swing.JSplitPane jSplitPane1;
   // End of variables declaration//GEN-END:variables

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("Metadata Nodes Overview");
   }

   private void showStatusDetails()
   {
      ArrayList<TreeMap<String, String>> statusVals;

      try
      {
         statusVals = this.updateThread.getParser().getVectorOfAttributeTreeMaps("status");

         JInternalFrameNodeStatus frame = new JInternalFrameNodeStatus(NodeTypesEnum.METADATA,
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
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
         {
            ex,
            true
         });
      }
   }
}

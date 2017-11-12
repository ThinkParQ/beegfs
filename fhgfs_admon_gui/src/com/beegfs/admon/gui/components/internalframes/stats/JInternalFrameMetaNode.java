package com.beegfs.admon.gui.components.internalframes.stats;

import com.beegfs.admon.gui.common.ValueUnit;
import com.beegfs.admon.gui.common.enums.ColorLineTraceEnum;
import com.beegfs.admon.gui.common.enums.FilePathsEnum;
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


public class JInternalFrameMetaNode extends javax.swing.JInternalFrame implements
        JInternalFrameInterface
{
   private static final Logger LOGGER = Logger.getLogger(
      JInternalFrameMetaNode.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "MetaNode";

   private static final int DEFAULT_TRACE_COUNT = 1;

   private final transient Node node;

   private transient MetaNodeUpdateThread updateThread;
   private LineChart workRequestsChart;
   private LineChart queuedWorkRequestsChart;


   public String getNodeID()
   {
      return node.getNodeID();
   }

   public int getNodeNumID()
   {
      return node.getNodeNumID();
   }

   /**
    * Creates new form JInternalFrameMetaNode
    * @param node The node object of the node to show the storage statistics.
    */
   public JInternalFrameMetaNode(Node node)
   {
      this.node = node;
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
   }

   private void startUpdateThread()
   {
      updateThread = new MetaNodeUpdateThread("XML_Metanode", THREAD_NAME + node.getNodeID());
      updateThread.start();
   }

   private void stopUpdateThread()
   {
      updateThread.shouldStop();
   }

   private class MetaNodeUpdateThread extends UpdateThread
   {
      private static final int DEFAULT_PARAMETER_COUNT = 3;
      private final String xmlPage;
      private final HashMap<String, String> parameters;

      MetaNodeUpdateThread(String parserUrl, String name)
      {
         super(parserUrl, name);
         xmlPage = parserUrl;
         parameters = new HashMap<>(DEFAULT_PARAMETER_COUNT);
         parameters.put("node", getNodeID());
         parameters.put("nodeNumID", String.valueOf(getNodeNumID()));
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
            paramCount++;
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

                  jPanelGeneral.revalidate();
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
            catch (CommunicationException | java.lang.NullPointerException e)
            {
               LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
               {
                  e,
                  true
               });
            }
            catch (java.lang.InterruptedException e)
            {
               LOGGER.log(Level.FINEST, "Internal error.", e);
            }
         }
         parser.shouldStop();
      }
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      if (!(obj instanceof JInternalFrameMetaNode))
      {
         return false;
      }
      JInternalFrameMetaNode objCasted = (JInternalFrameMetaNode) obj;
      return objCasted.getNodeNumID() == getNodeNumID();
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
      jPanelWorkRequests = new javax.swing.JPanel();
      jSplitPane1 = new javax.swing.JSplitPane();
      jPanelWorkRequestsGraph = new javax.swing.JPanel();
      jPanelQueuedWorkRequestsGraph = new javax.swing.JPanel();
      jPanelButtons = new javax.swing.JPanel();
      jLabelTimeSpan = new javax.swing.JLabel();
      jComboBoxTimeSpan = new javax.swing.JComboBox<>();
      jPanelGeneral = new javax.swing.JPanel();
      jLabelNodeIDText = new javax.swing.JLabel();
      jLabelNodeID = new javax.swing.JLabel();
      jLabelStatusText = new javax.swing.JLabel();
      jLabelStatus = new javax.swing.JLabel();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setPreferredSize(new java.awt.Dimension(824, 715));
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
      jPanelFrame.setPreferredSize(new java.awt.Dimension(768, 677));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jPanelWorkRequests.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Work Requests")));
      jPanelWorkRequests.setLayout(new java.awt.BorderLayout());

      jSplitPane1.setDividerLocation(300);
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

      jSplitPane1.setLeftComponent(jPanelWorkRequestsGraph);

      jPanelQueuedWorkRequestsGraph.setPreferredSize(new java.awt.Dimension(300, 300));
      jPanelQueuedWorkRequestsGraph.setLayout(new java.awt.GridLayout(1, 0));

      LineTrace queuedWorkRequestsTrace = new LineTrace(Main.getLocal().getString("Queued Work Requests"),
         ColorLineTraceEnum.COLOR_META_OPS.color(), workRequests);

      LineTrace[] queuedWorkRequestTraces = new LineTrace[DEFAULT_TRACE_COUNT];
      queuedWorkRequestTraces[0] =  queuedWorkRequestsTrace;

      queuedWorkRequestsChart = new LineChart(Main.getLocal().getString("Queued Work Requests"), "", "", 600,
         queuedWorkRequestTraces, false, false);

      jPanelQueuedWorkRequestsGraph.add(queuedWorkRequestsChart);

      jSplitPane1.setRightComponent(jPanelQueuedWorkRequestsGraph);

      jPanelWorkRequests.add(jSplitPane1, java.awt.BorderLayout.CENTER);

      jPanelButtons.setLayout(new java.awt.FlowLayout(java.awt.FlowLayout.LEFT));

      jLabelTimeSpan.setText(Main.getLocal().getString("Show the last "));
      jPanelButtons.add(jLabelTimeSpan);

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
      jPanelButtons.add(jComboBoxTimeSpan);

      jPanelWorkRequests.add(jPanelButtons, java.awt.BorderLayout.NORTH);

      jPanelFrame.add(jPanelWorkRequests, java.awt.BorderLayout.CENTER);

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
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 813, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 685, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      this.stopUpdateThread();
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
      this.startUpdateThread();
   }//GEN-LAST:event_formInternalFrameOpened

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JComboBox<String> jComboBoxTimeSpan;
   private javax.swing.JLabel jLabelNodeID;
   private javax.swing.JLabel jLabelNodeIDText;
   private javax.swing.JLabel jLabelStatus;
   private javax.swing.JLabel jLabelStatusText;
   private javax.swing.JLabel jLabelTimeSpan;
   private javax.swing.JPanel jPanelButtons;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelGeneral;
   private javax.swing.JPanel jPanelQueuedWorkRequestsGraph;
   private javax.swing.JPanel jPanelWorkRequests;
   private javax.swing.JPanel jPanelWorkRequestsGraph;
   private javax.swing.JScrollPane jScrollPaneFrame;
   private javax.swing.JSplitPane jSplitPane1;
   // End of variables declaration//GEN-END:variables

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("Metadata Node: ") + node.getTypedNodeID();
   }
}

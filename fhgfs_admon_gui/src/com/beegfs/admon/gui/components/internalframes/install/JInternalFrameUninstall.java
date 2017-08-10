package com.beegfs.admon.gui.components.internalframes.install;

import com.beegfs.admon.gui.common.XMLParser;
import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.GuiThread;
import com.beegfs.admon.gui.common.threading.ProgressBarThread;
import com.beegfs.admon.gui.common.tools.CryptTk;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.dialogs.JDialogUninstStatus;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.components.tables.InstallTableCellRenderer;
import com.beegfs.admon.gui.program.Main;
import com.myjavatools.web.ClientHttpRequest;
import java.awt.Component;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.TreeMap;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JOptionPane;
import javax.swing.JTable;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.JTableHeader;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;
import javax.swing.table.TableColumnModel;


public class JInternalFrameUninstall extends javax.swing.JInternalFrame implements
   JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(JInternalFrameUninstall.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "Uninstall";

   private ArrayList<Object[]> mgmtdInfo;
   private ArrayList<Object[]> metaInfo;
   private ArrayList<Object[]> storageInfo;
   private ArrayList<Object[]> clientInfo;

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("Uninstall BeeGFS");
   }


   private static class OverviewTable extends JTable
   {
      private static final long serialVersionUID = 1L;

      OverviewTable()
      {
         super();
         DefaultTableModel defModel = new DefaultTableModelImpl();
         defModel.addColumn(Main.getLocal().getString("Type"));
         defModel.addColumn(Main.getLocal().getString("Host"));
         defModel.addColumn(Main.getLocal().getString("Architecture"));
         defModel.addColumn(Main.getLocal().getString("Distribution"));
         setModel(defModel);
         InstallTableCellRenderer renderer = new InstallTableCellRenderer();
         getColumnModel().getColumn(0).setCellRenderer(renderer);
         getColumnModel().getColumn(1).setCellRenderer(renderer);
         getColumnModel().getColumn(2).setCellRenderer(renderer);
         getColumnModel().getColumn(3).setCellRenderer(renderer);
         setColumnSelectionAllowed(false);
         setCellSelectionEnabled(false);
         getTableHeader().setReorderingAllowed(false);
         getTableHeader().setResizingAllowed(true);
         setAutoCreateRowSorter(true);
         setCellEditor(null);
      }


      private static class DefaultTableModelImpl extends DefaultTableModel
      {
         private static final long serialVersionUID = 1L;

         DefaultTableModelImpl()
         {
         }

         @Override
         public boolean isCellEditable(int row, int col)
         {
            return false;
         }

      }
   }

   private class UninstallThread extends GuiThread
   {
      private final JDialogUninstStatus dialog;

      UninstallThread(JDialogUninstStatus dialog)
      {
         super(THREAD_NAME);
         this.dialog = dialog;
      }

      public JDialogUninstStatus getDialog()
      {
         return dialog;
      }

      @Override
      public void run()
      {
         boolean errorOccured = false;
         ProgressBarThread pBarThread = new ProgressBarThread(this.dialog.getProgressBar(),
            THREAD_NAME + Main.getLocal().getString("ProgressBar"));
         pBarThread.start();
         createNewSetupLogfile();
         printUninstallSummary();

         getDialog().addLine(Main.getLocal().getString("Check ssh connection to hosts..."), false);

         if (checkSSH())
         {
        	 getDialog().addLine(Main.getLocal().getString("Connection to all hosts available."), false);
            if (!stop)
            {
            	errorOccured = errorOccured || doUninstall(Main.getLocal().getString("client"), Main.getLocal().getString("BeeGFS client"));
            }
            if (!stop)
            {
            	errorOccured = errorOccured || doUninstall(Main.getLocal().getString("meta"), Main.getLocal().getString("BeeGFS meta server"));
            }
            if (!stop)
            {
            	errorOccured = errorOccured || doUninstall(Main.getLocal().getString("storage"), Main.getLocal().getString("BeeGFS storage server"));
            }
            if (!stop)
            {
            	errorOccured = errorOccured || doUninstall(Main.getLocal().getString("mgmtd"), Main.getLocal().getString("BeeGFS management daemon"));
            }
            if (!stop)
            {
            	errorOccured = errorOccured || doUninstall(Main.getLocal().getString("opentk"), Main.getLocal().getString("BeeGFS open TK library"));
            }
         }
         else
         {
        	 getDialog().addLine(Main.getLocal().getString("Connection to hosts failed."), true);
         }

         pBarThread.shouldStop();

         if (!errorOccured)
         {
            this.getDialog().setFinished();
         }
      }

      private void createNewSetupLogfile()
      {
         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();
            int nonceID = Integer.parseInt(parser.getValue("id"));
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_CreateNewSetupLogfile");
            String param = "?nonceID=" + nonceID + "&secret=" + secret + "&type=uninstall";
            parser.setUrl(url + param);
            parser.update();
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
            {
               e,
               true
            });
            this.getDialog().addLine(Main.getLocal().getString("Uninstallation cannot be performed due to communication ") +
                Main.getLocal().getString("problems"));
            this.getDialog().setFinished();
            this.shouldStop();
         }
      }

      private void printUninstallSummary()
      {
         String mgmtdString = "";
         String metaString = "";
         String storageString = "";
         String clientsString = "";

         for (Object[] nodeInfo : mgmtdInfo)
         {
            if (mgmtdString.isEmpty())
            {
               mgmtdString = (String) nodeInfo[1];
            }
            else
            {
               mgmtdString += ", " + nodeInfo[1];
            }
         }

         for (Object[] nodeInfo : metaInfo)
         {
            if (metaString.isEmpty())
            {
               metaString = (String) nodeInfo[1];
            }
            else
            {
               metaString += ", " + nodeInfo[1];
            }
         }

         for (Object[] nodeInfo : storageInfo)
         {
            if (storageString.isEmpty())
            {
               storageString = (String) nodeInfo[1];
            }
            else
            {
               storageString += ", " + nodeInfo[1];
            }
         }

         for (Object[] nodeInfo : clientInfo)
         {
            if (clientsString.isEmpty())
            {
               clientsString = (String) nodeInfo[1];
            }
            else
            {
               clientsString += ", " + nodeInfo[1];
            }
         }

         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine(Main.getLocal().getString("starting uninstallation..."));
         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine(Main.getLocal().getString("Managment server: ") + mgmtdString);
         this.getDialog().addLine(Main.getLocal().getString("Metadata server: ") + metaString);
         this.getDialog().addLine(Main.getLocal().getString("Storage server: ") + storageString);
         this.getDialog().addLine(Main.getLocal().getString("Clients: ") + clientsString);
         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine(" ");

         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, Main.getLocal().getString("starting uninstallation..."), true);
         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, Main.getLocal().getString("Managment server: ") + mgmtdString, true);
         LOGGER.log(Level.INFO, Main.getLocal().getString("Metadata server: ") + metaString, true);
         LOGGER.log(Level.INFO, Main.getLocal().getString("Storage server: ") + storageString, true);
         LOGGER.log(Level.INFO, Main.getLocal().getString("Clients: ") + clientsString, true);
         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, " ", true);
      }

      private boolean doUninstall(String packageID, String packageDesc)
      {
         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();
            int nonceID = Integer.parseInt(parser.getValue("id"));
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_Uninstall");
            String param = "?package=" + packageID + "&nonceID=" + nonceID + "&secret=" +
               secret;
            parser.setUrl(url + param);
            parser.update();
            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (authenticated)
            {
               ArrayList<String> failed = parser.getVector("failedHosts");
               if (failed.isEmpty())
               {
                  this.getDialog().addLine(Main.getLocal().getString("Successfully removed ") + packageDesc +
                     Main.getLocal().getString("from all hosts"), false);
                  return false;
               }
               else
               {
                  for (String node : failed)
                  {
                     this.getDialog().addLine(Main.getLocal().getString("Failed to remove ") + packageDesc +
                        Main.getLocal().getString("from host ") + node, true);
                  }
                  return true;
               }
            }
            else
            {
               this.getDialog().addLine(Main.getLocal().getString("Removal cannot be performed due to failed ") +
                  Main.getLocal().getString("authentication"));
               this.getDialog().setFinished();
               this.shouldStop();
               LOGGER.log(Level.SEVERE, Main.getLocal().getString("Authentication failed. You are not allowed to perform ") +
                  Main.getLocal().getString("this operation!"), true);
               return true;
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
            {
               e,
               true
            });
            this.getDialog().addLine(Main.getLocal().getString("Removal cannot be performed due to communication ") +
               Main.getLocal().getString("problems"));
            this.getDialog().setFinished();
            this.shouldStop();
            return true;
         }
      }

   }

   @SuppressWarnings("FinallyDiscardsException")
   private boolean checkSSH()
   {
      boolean retVal = true;
      InputStream serverResp = null;

      try
      {
         String url = HttpTk.generateAdmonUrl("/XML_CheckSSH");

         int dataSize = metaInfo.size() + storageInfo.size() + clientInfo.size();
         int objSize = (2 * dataSize) + 2;

         Object[] obj = new Object[objSize];
         obj[0] = "node";
         obj[1] = mgmtdInfo.get(0)[1];

         int objCounter = 2;
         int i;

         for (i = 0; i < metaInfo.size(); i++)
         {
            obj[objCounter] = "node";
            objCounter++;
            String node = (String) metaInfo.get(i)[1];
            obj[objCounter] = node;
            objCounter++;
         }
         for (i = 0; i < storageInfo.size(); i++)
         {
            obj[objCounter] = "node";
            objCounter++;
            String node = (String) storageInfo.get(i)[1];
            obj[objCounter] = node;
            objCounter++;
         }
         for (i = 0; i < clientInfo.size(); i++)
         {
            obj[objCounter] = "node";
            objCounter++;
            String node = (String) clientInfo.get(i)[1];
            obj[objCounter] = node;
            objCounter++;
         }
         serverResp = ClientHttpRequest.post(new java.net.URL(url), obj);
         XMLParser parser = new XMLParser(THREAD_NAME);
         parser.readFromInputStream(serverResp);
         ArrayList<String> failedNodes = parser.getVector();
         failedNodes.remove("");
         if (!failedNodes.isEmpty())
         {
            StringBuilder msg = new StringBuilder(Main.getLocal().getString("Some hosts are not reachable through SSH.") +
               System.lineSeparator() + Main.getLocal().getString("Please make sure that all hosts exist and user root is ") +
               Main.getLocal().getString("able to do ") + Main.getLocal().getString("passwordless SSH login.") + System.lineSeparator() +
               System.lineSeparator() + Main.getLocal().getString("Failed Hosts : ") + System.lineSeparator() +
               System.lineSeparator());
            java.util.HashSet<String> tmpSet = new java.util.HashSet<>(failedNodes.size());
            for (String n : failedNodes)
            {
               if (!tmpSet.contains(n))
               {
                  msg.append(n);
                  msg.append(System.lineSeparator());
                  tmpSet.add(n);
               }
            }
            JOptionPane.showMessageDialog(this, msg, Main.getLocal().getString("Error"), JOptionPane.ERROR_MESSAGE);
            retVal = false;
         }
      }
      catch (IOException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("IO error"), e);
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
         try
         {
            if (serverResp != null)
            {
               serverResp.close();
            }

            return retVal;
         }
         catch (IOException e)
         {
            LOGGER.log(Level.SEVERE, "IO error", e);
         }
      }
      return retVal;
   }

   /**
    * Creates new form JInternalFrameMetaNode
    */
   public JInternalFrameUninstall()
   {
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
      loadInfo();
   }

   private void loadInfo()
   {
      XMLParser parser = new XMLParser(HttpTk.generateAdmonUrl("/XML_InstallInfo"), THREAD_NAME);
      parser.update();
      try
      {
         ArrayList<TreeMap<String, String>> mgmtdNodes =
            parser.getVectorOfAttributeTreeMaps("mgmtd");
         ArrayList<TreeMap<String, String>> metaNodes =
            parser.getVectorOfAttributeTreeMaps("meta");
         ArrayList<TreeMap<String, String>> storageNodes =
            parser.getVectorOfAttributeTreeMaps("storage");
         ArrayList<TreeMap<String, String>> clientNodes =
            parser.getVectorOfAttributeTreeMaps("client");

         mgmtdInfo = new ArrayList<>(mgmtdNodes.size());
         for (TreeMap<String, String> nodeInfo : mgmtdNodes)
         {
            String nodeID = nodeInfo.get("value");
            String arch = nodeInfo.get("arch");
            String dist = nodeInfo.get("dist");
            Object[] obj = new Object[]
            {
               NodeTypesEnum.MANAGMENT.type(),
               nodeID,
               arch,
               dist
            };
            mgmtdInfo.add(obj);
         }

         metaInfo = new ArrayList<>(metaNodes.size());
         for (TreeMap<String, String> nodeInfo : metaNodes)
         {
            String nodeID = nodeInfo.get("value");
            String arch = nodeInfo.get("arch");
            String dist = nodeInfo.get("dist");
            Object[] obj = new Object[]
            {
               NodeTypesEnum.METADATA.type(),
               nodeID,
               arch,
               dist
            };
            metaInfo.add(obj);
         }

         storageInfo = new ArrayList<>(storageNodes.size());
         for (TreeMap<String, String> nodeInfo : storageNodes)
         {
            String nodeID = nodeInfo.get("value");
            String arch = nodeInfo.get("arch");
            String dist = nodeInfo.get("dist");
            Object[] obj = new Object[]
            {
               NodeTypesEnum.STORAGE.type(),
               nodeID,
               arch,
               dist
            };
            storageInfo.add(obj);
         }

         clientInfo = new ArrayList<>(clientNodes.size());
         for (TreeMap<String, String> nodeInfo : clientNodes)
         {
            String nodeID = nodeInfo.get("value");
            String arch = nodeInfo.get("arch");
            String dist = nodeInfo.get("dist");
            Object[] obj = new Object[]
            {
               NodeTypesEnum.CLIENT.type(),
               nodeID,
               arch,
               dist
            };
            clientInfo.add(obj);
         }

         loadTable(mgmtdInfo, metaInfo, storageInfo, clientInfo);
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]
         {
            e,
            true
         });
      }
   }

   private static int calcColumnWidths(JTable table)
   {
      JTableHeader header = table.getTableHeader();
      TableCellRenderer defaultHeaderRenderer = null;

      if (header != null)
      {
         defaultHeaderRenderer = header.getDefaultRenderer();
      }

      TableColumnModel columns = table.getColumnModel();
      DefaultTableModel data = (DefaultTableModel) table.getModel();
      int rowCount = data.getRowCount();
      int totalWidth = 0;

      for (int i = columns.getColumnCount() - 1; i >= 0; --i)
      {
         TableColumn column = columns.getColumn(i);
         int columnIndex = column.getModelIndex();
         int width = -1;

         TableCellRenderer h = column.getHeaderRenderer();
         if (h == null)
         {
            h = defaultHeaderRenderer;
         }

         if (h != null)
         {
            Component c = h.getTableCellRendererComponent(table, column.getHeaderValue(),
               false, false, -1, i);
            width = c.getPreferredSize().width;
         }

         for (int row = rowCount - 1; row >= 0; --row)
         {
            TableCellRenderer r = table.getCellRenderer(row, i);
            Component c = r.getTableCellRendererComponent(table, data.getValueAt(row,
               columnIndex), false, false, row, i);
            width = Math.max(width, c.getPreferredSize().width);
         }

         if (width >= 0)
         {
            column.setPreferredWidth(width); // <1.3: without margin
         }

         totalWidth += column.getPreferredWidth();
      }
      return totalWidth;
   }

   private void loadTable(ArrayList<Object[]> mgmtdInfo, ArrayList<Object[]> metaInfo,
      ArrayList<Object[]> storageInfo, ArrayList<Object[]> clientInfo)
   {
      DefaultTableModel model = (DefaultTableModel) (jTableUninstall.getModel());
      model.setRowCount(0);
      for (Object[] obj : mgmtdInfo)
      {
         model.addRow(obj);
      }
      for (Object[] obj : metaInfo)
      {
         model.addRow(obj);
      }
      for (Object[] obj : storageInfo)
      {
         model.addRow(obj);
      }
      for (Object[] obj : clientInfo)
      {
         model.addRow(obj);
      }

      calcColumnWidths(jTableUninstall);
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameUninstall;
   }

    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    @SuppressWarnings("unchecked")
   // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
   private void initComponents()
   {

      jScrollPaneFrame = new javax.swing.JScrollPane();
      jPanelFrame = new javax.swing.JPanel();
      jTextAreaDescription = new javax.swing.JTextArea();
      jScrollPaneUninstall = new javax.swing.JScrollPane();
      jTableUninstall = new OverviewTable();
      jPanelButtons = new javax.swing.JPanel();
      jButtonReload = new javax.swing.JButton();
      filler1 = new javax.swing.Box.Filler(new java.awt.Dimension(500, 30), new java.awt.Dimension(500, 30), new java.awt.Dimension(500, 30));
      jButtonInstall = new javax.swing.JButton();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      addInternalFrameListener(new javax.swing.event.InternalFrameListener()
      {
         public void internalFrameOpened(javax.swing.event.InternalFrameEvent evt)
         {
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
      jPanelFrame.setPreferredSize(new java.awt.Dimension(801, 646));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jTextAreaDescription.setEditable(false);
      jTextAreaDescription.setBackground(new java.awt.Color(238, 238, 238));
      jTextAreaDescription.setColumns(20);
      jTextAreaDescription.setLineWrap(true);
      jTextAreaDescription.setRows(5);
      jTextAreaDescription.setText(Main.getLocal().getString("Uninstall BeeGFS"));
      jTextAreaDescription.setWrapStyleWord(true);
      jTextAreaDescription.setBorder(null);
      jPanelFrame.add(jTextAreaDescription, java.awt.BorderLayout.NORTH);

      jTableUninstall.setModel(jTableUninstall.getModel());
      jTableUninstall.getTableHeader().setReorderingAllowed(false);
      jScrollPaneUninstall.setViewportView(jTableUninstall);

      jPanelFrame.add(jScrollPaneUninstall, java.awt.BorderLayout.CENTER);

      jButtonReload.setText(Main.getLocal().getString("Reload"));
      jButtonReload.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonReloadActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonReload);
      jPanelButtons.add(filler1);

      jButtonInstall.setText(Main.getLocal().getString("Uninstall"));
      jButtonInstall.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonInstallActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonInstall);

      jPanelFrame.add(jPanelButtons, java.awt.BorderLayout.SOUTH);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 811, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 662, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      //  parser.shouldStop();
      FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

    private void jButtonInstallActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonInstallActionPerformed
      if (rolesDefined())
      {
         doUninstall();
      }
      else
      {
    	  JOptionPane.showMessageDialog(this, Main.getLocal().getString("Setup cannot determine services!"), Main.getLocal().getString("Server roles ") +
			Main.getLocal().getString("undefined"), JOptionPane.ERROR_MESSAGE);
      }
    }//GEN-LAST:event_jButtonInstallActionPerformed

    private void jButtonReloadActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonReloadActionPerformed
      loadInfo();
    }//GEN-LAST:event_jButtonReloadActionPerformed

   private void doUninstall()
   {
      JDialogUninstStatus jDialogUninstStatus = new JDialogUninstStatus();
      jDialogUninstStatus.setVisible(true);
      UninstallThread uninstallThread = new UninstallThread(jDialogUninstStatus);
      jDialogUninstStatus.setManagementThread(uninstallThread);
      uninstallThread.start();
   }

   private boolean rolesDefined()
   {
      return (!mgmtdInfo.isEmpty()) && (!metaInfo.isEmpty()) && (!storageInfo.isEmpty()) &&
         (!clientInfo.isEmpty());
   }
   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.Box.Filler filler1;
   private javax.swing.JButton jButtonInstall;
   private javax.swing.JButton jButtonReload;
   private javax.swing.JPanel jPanelButtons;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JScrollPane jScrollPaneFrame;
   private javax.swing.JScrollPane jScrollPaneUninstall;
   private javax.swing.JTable jTableUninstall;
   private javax.swing.JTextArea jTextAreaDescription;
   // End of variables declaration//GEN-END:variables
}

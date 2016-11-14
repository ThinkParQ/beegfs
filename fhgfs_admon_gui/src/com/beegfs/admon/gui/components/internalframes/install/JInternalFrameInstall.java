package com.beegfs.admon.gui.components.internalframes.install;


import com.beegfs.admon.gui.common.XMLParser;
import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import com.beegfs.admon.gui.common.enums.PropertyEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.GuiThread;
import com.beegfs.admon.gui.common.threading.ProgressBarThread;
import com.beegfs.admon.gui.common.tools.CryptTk;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.dialogs.JDialogEULA;
import com.beegfs.admon.gui.components.dialogs.JDialogInstStatus;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.components.tables.InstallTableCellRenderer;
import com.beegfs.admon.gui.program.Main;
import com.myjavatools.web.ClientHttpRequest;
import java.awt.Component;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashSet;
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


public class JInternalFrameInstall extends javax.swing.JInternalFrame implements
   JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(JInternalFrameInstall.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "Install";

   private static final int DEFAULT_NODE_CAPACITY = 20;
   
   private ArrayList<Object[]> mgmtdInfo;
   private ArrayList<Object[]> metaInfo;
   private ArrayList<Object[]> storageInfo;
   private ArrayList<Object[]> clientInfo;

   @Override
   public final String getFrameTitle()
   {
      return "Install BeeGFS";
   }

   private static class OverviewTable extends JTable
   {
      private static final long serialVersionUID = 1L;

      OverviewTable()
      {
         super();

         DefaultTableModel defModel = new DefaultTableModelImpl();
         defModel.addColumn("Type");
         defModel.addColumn("Host");
         defModel.addColumn("Architecture");
         defModel.addColumn("Distribution");
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

   private class InstallThread extends GuiThread
   {
      private final JDialogInstStatus dialog;

      InstallThread(JDialogInstStatus dialog)
      {
         super(THREAD_NAME);
         this.dialog = dialog;
      }

      public JDialogInstStatus getDialog()
      {
         return dialog;
      }

      @Override
      public void run()
      {
         boolean errorOccured = false;
         ProgressBarThread pBarThread = new ProgressBarThread(this.dialog.getProgressBar(),
            THREAD_NAME + "ProgressBar");
         pBarThread.start();
         createNewSetupLogfile();
         printInstallSummary();

         getDialog().addLine("Check ssh connection to hosts...", false);

         if (checkSSH())
         {
            getDialog().addLine("Connection to all hosts available.", false);
            
            if (!stop)
            {
               errorOccured = errorOccured || doInstall("mgmtd", "BeeGFS management daemon");
            }
            if (!stop)
            {
               errorOccured = errorOccured || doInstall("meta", "BeeGFS meta server");
            }
            if (!stop)
            {
               errorOccured = errorOccured || doInstall("storage", "BeeGFS storage server");
            }
            if (!stop)
            {
               errorOccured = errorOccured || doInstall("client", "BeeGFS client");
            }

         }
         else
         {
            getDialog().addLine("Connection to hosts failed.", true);
         }

         errorOccured = errorOccured || this.checkClientDependencies();
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
            String param = "?nonceID=" + nonceID + "&secret=" + secret + "&type=install";
            parser.setUrl(url + param);
            parser.update();
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
            {
               e,
               true
            });
            this.getDialog().addLine(
               "Installation cannot be performed due to communication problems");
            this.getDialog().setFinished();
            this.shouldStop();
         }
      }

      private void printInstallSummary()
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
         this.getDialog().addLine("starting installation...");
         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine("Managment server: " + mgmtdString);
         this.getDialog().addLine("Metadata server: " + metaString);
         this.getDialog().addLine("Storage server: " + storageString);
         this.getDialog().addLine("Clients: " + clientsString);
         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine("--------------------------------------------");
         this.getDialog().addLine(" ");

         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, "starting installation...", true);
         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, "Managment server: " + mgmtdString, true);
         LOGGER.log(Level.INFO, "Metadata server: " + metaString, true);
         LOGGER.log(Level.INFO, "Storage server: " + storageString, true);
         LOGGER.log(Level.INFO, "Clients: " + clientsString, true);
         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, "--------------------------------------------", true);
         LOGGER.log(Level.INFO, " ", true);
      }

      private boolean checkClientDependencies()
      {
         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetCheckClientDependenciesLog");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();

            boolean success = Boolean.parseBoolean(parser.getValue("success"));
            if (success)
            {
               String hostList = parser.getValue("log");
               if (!hostList.isEmpty())
               {
                  this.getDialog().addLine(" ");
                  this.getDialog().addLine("--------------------------------------------");
                  this.getDialog().addLine(
                     "WARNING: Linux kernel build directory not found. Please " +
                      "check if the kernel module development packages are installed " +
                      "for the current kernel version. (RHEL: kernel-devel; SLES: " +
                      "linux-kernel-headers, kernel-source; Debian: linux-headers). If " +
                      "a custom kernel is used ignore this warning!");
                  this.getDialog().addLine("The kernel build directory is missing on the " +
                      "following nodes:" + System.getProperty(
                         PropertyEnum.PROPERTY_LINE_SEPARATOR.getKey()) + hostList);
                  this.getDialog().addLine("--------------------------------------------");
               }
            }
            else
            {
               LOGGER.log(Level.WARNING, "Couldn't get client dependency check results!", true);
               this.getDialog().addLine("Couldn't get client dependency check results!");
               this.getDialog().setFinished();
               this.shouldStop();
               return true;
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
            {
               e,
               true
            });
            this.getDialog().addLine(
               "Installation cannot be performed due to communication problems");
            this.getDialog().setFinished();
            this.shouldStop();
            return true;
         }
         return false;
      }

      private boolean doInstall(String packageID, String packageDesc)
      {
         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();
            int nonceID = Integer.parseInt(parser.getValue("id"));
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_Install");
            String param = "?package=" + packageID + "&nonceID=" + nonceID + "&secret=" + secret;
            parser.setUrl(url + param);
            parser.update();
            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (authenticated)
            {
               ArrayList<String> failed = parser.getVector("failedHosts");
               if (failed.isEmpty())
               {
                  this.getDialog().
                     addLine("Successfully installed " + packageDesc + " on all hosts", false);
                  return false;
               }
               else
               {
                  for (String node : failed)
                  {
                     this.getDialog().addLine("Failed to install " + packageDesc + " on host " +
                        node, true);
                  }
                  return true;
               }
            }
            else
            {
               this.getDialog().addLine(
                  "Installation cannot be performed due to failed authentication");
               this.getDialog().setFinished();
               this.shouldStop();
               LOGGER.log(Level.SEVERE,
                  "Authentication failed. You are not allowed to perform an installation!", true);
               return true;
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
            {
               e,
               true
            });
            this.getDialog().addLine(
               "Installation cannot be performed due to communication problems");
            this.getDialog().setFinished();
            this.shouldStop();
            return true;
         }
      }

   }

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
            StringBuilder msg = new StringBuilder("Some hosts are not reachable through SSH." +
               System.lineSeparator() + "Please make sure that all hosts exist and user root is " +
               "able to do passwordless SSH login." + System.lineSeparator() +
               System.lineSeparator() + "Failed Hosts :" + System.lineSeparator() +
               System.lineSeparator());
            HashSet<String> tmpSet = new HashSet<>(failedNodes.size());
            for (String n : failedNodes)
            {
               if (!tmpSet.contains(n))
               {
                  msg.append(n);
                  msg.append(System.lineSeparator());
                  tmpSet.add(n);
               }
            }
            JOptionPane.showMessageDialog(this, msg, "Error", JOptionPane.ERROR_MESSAGE);
            retVal = false;
         }

         return retVal;
      }
      catch (IOException e)
      {
         LOGGER.log(Level.SEVERE, "IO error", e);
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
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
   public JInternalFrameInstall()
   {
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());

      mgmtdInfo = new ArrayList<>(DEFAULT_NODE_CAPACITY);
      metaInfo = new ArrayList<>(DEFAULT_NODE_CAPACITY);
      storageInfo = new ArrayList<>(DEFAULT_NODE_CAPACITY);
      clientInfo = new ArrayList<>(DEFAULT_NODE_CAPACITY);

      loadInfo();
   }

   private void loadInfo()
   {
      XMLParser parser = new XMLParser(HttpTk.generateAdmonUrl("/XML_InstallInfo"), THREAD_NAME);
      parser.update();
      try
      {
         ArrayList<TreeMap<String, String>> mgmtdNodes = parser.getVectorOfAttributeTreeMaps(
            "mgmtd");
         ArrayList<TreeMap<String, String>> metaNodes = parser.getVectorOfAttributeTreeMaps("meta");
         ArrayList<TreeMap<String, String>> storageNodes = parser.getVectorOfAttributeTreeMaps(
            "storage");
         ArrayList<TreeMap<String, String>> clientNodes = parser.getVectorOfAttributeTreeMaps(
            "client");

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
         LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
         {
            e,
            true
         });
         mgmtdInfo = new ArrayList<>(0);
         metaInfo = new ArrayList<>(0);
         storageInfo = new ArrayList<>(0);
         clientInfo = new ArrayList<>(0);
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
            Component c = r.getTableCellRendererComponent(table, data.getValueAt(row, columnIndex),
               false, false, row, i);
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
      DefaultTableModel model = (DefaultTableModel) (jTableInstall.getModel());
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

      calcColumnWidths(jTableInstall);
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameInstall;
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
      jScrollPaneTableInstall = new javax.swing.JScrollPane();
      jTableInstall = new OverviewTable();
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
      jTextAreaDescription.setText("Install BeeGFS\n\nBased on the information provided in the previous steps, an automatic installation of BeeGFS is performed now. Please check the data gathered about your nodes before you continue.");
      jTextAreaDescription.setWrapStyleWord(true);
      jTextAreaDescription.setBorder(null);
      jPanelFrame.add(jTextAreaDescription, java.awt.BorderLayout.NORTH);

      jScrollPaneTableInstall.setMinimumSize(new java.awt.Dimension(0, 0));
      jScrollPaneTableInstall.setPreferredSize(new java.awt.Dimension(0, 0));

      jTableInstall.setModel(jTableInstall.getModel());
      jTableInstall.getTableHeader().setReorderingAllowed(false);
      jScrollPaneTableInstall.setViewportView(jTableInstall);

      jPanelFrame.add(jScrollPaneTableInstall, java.awt.BorderLayout.CENTER);

      jPanelButtons.setMinimumSize(new java.awt.Dimension(789, 40));
      jPanelButtons.setPreferredSize(new java.awt.Dimension(789, 40));

      jButtonReload.setText("Reload");
      jButtonReload.setMaximumSize(new java.awt.Dimension(80, 30));
      jButtonReload.setMinimumSize(new java.awt.Dimension(80, 30));
      jButtonReload.setPreferredSize(new java.awt.Dimension(80, 30));
      jButtonReload.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonReloadActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonReload);
      jPanelButtons.add(filler1);

      jButtonInstall.setText("Install");
      jButtonInstall.setMaximumSize(new java.awt.Dimension(80, 30));
      jButtonInstall.setMinimumSize(new java.awt.Dimension(80, 30));
      jButtonInstall.setPreferredSize(new java.awt.Dimension(80, 30));
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
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 665, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      //  parser.shouldStop();
      FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

    private void jButtonInstallActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonInstallActionPerformed
      checkConfigurationSaved();
      if (rolesDefined())
      {
         JDialogEULA eulaDialog = new JDialogEULA(Main.getMainWindow(), true);
         eulaDialog.getScrollPane().getVerticalScrollBar().setValue(0);
         eulaDialog.getScrollPane().getHorizontalScrollBar().setValue(0);
         boolean eulaConfirmed = eulaDialog.showDialog();
         if (eulaConfirmed)
         {
            doInstall();
         }
      }
      else
      {
         JOptionPane.showMessageDialog(this, "You must define the server roles before installing"
            + " BeeGFS", "Server roles undefined", JOptionPane.ERROR_MESSAGE);
      }
    }//GEN-LAST:event_jButtonInstallActionPerformed

    private void jButtonReloadActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonReloadActionPerformed
      checkConfigurationSaved();
      loadInfo();
    }//GEN-LAST:event_jButtonReloadActionPerformed

   private boolean checkConfigurationSaved()
   {
      JInternalFrameInstallationConfig testFrame = new JInternalFrameInstallationConfig();
      boolean isConfigureFrameOpen = FrameManager.isFrameOpen(testFrame, true);
      boolean isConfigSaved;

      if (isConfigureFrameOpen)
      {
         JInternalFrameInstallationConfig installFrame =
            (JInternalFrameInstallationConfig) FrameManager.getOpenFrame(testFrame, true);
         isConfigSaved = installFrame.checkWholeConfigSaved();
         testFrame.dispose();
         return isConfigSaved;
      }
      return true;
   }

   private void doInstall()
   {
      JDialogInstStatus jDialogInstStatus = new JDialogInstStatus();
      jDialogInstStatus.setVisible(true);
      InstallThread installThread = new InstallThread(jDialogInstStatus);
      jDialogInstStatus.setManagementThread(installThread);
      installThread.start();
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
   private javax.swing.JScrollPane jScrollPaneTableInstall;
   private javax.swing.JTable jTableInstall;
   private javax.swing.JTextArea jTextAreaDescription;
   // End of variables declaration//GEN-END:variables
}

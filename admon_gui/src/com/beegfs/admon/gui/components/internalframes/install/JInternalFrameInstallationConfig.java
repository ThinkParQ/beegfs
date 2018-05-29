package com.beegfs.admon.gui.components.internalframes.install;

import com.beegfs.admon.gui.common.XMLParser;
import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import static com.beegfs.admon.gui.common.enums.FilePathsEnum.IMAGE_INFO;
import com.beegfs.admon.gui.common.enums.UpdateDataTypeEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.frames.JFrameAddHosts;
import com.beegfs.admon.gui.components.frames.JFrameFileChooser;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameUpdateableInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.program.Main;
import com.myjavatools.web.ClientHttpRequest;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.HeadlessException;
import java.awt.event.ActionEvent;
import java.awt.event.ItemEvent;
import java.awt.event.KeyEvent;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.DefaultListModel;
import javax.swing.GroupLayout;
import javax.swing.GroupLayout.Alignment;
import javax.swing.JCheckBox;
import javax.swing.JInternalFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.ListModel;


public class JInternalFrameInstallationConfig extends javax.swing.JInternalFrame implements
   JInternalFrameUpdateableInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameInstallationConfig.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "InstallConfig";

   private ArrayList<String> metaNodes;
   private ArrayList<String> storageNodes;
   private ArrayList<String> clientNodes;
   private ArrayList<String> mgmtdServers;
   private String mgmtdServer;
   private boolean isSavedRoles;
   private boolean isSavedConfig;
   private boolean isSavedIB;
   private boolean saveDefaultIBConfig;
   private int lastTab;

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("Installation -> Configuration");
   }

   @Override
   public void updateData(ArrayList<String> data, UpdateDataTypeEnum type)
   {
      DefaultListModel<String> listModel;

      if (null != type)
      {
         switch (type)
         {
            case HOSTS_CLIENT:
               listModel = (DefaultListModel<String>) jListRoleClient.getModel();
               break;
            case HOSTS_META:
               listModel = (DefaultListModel<String>) jListRoleMeta.getModel();
               break;
            default:
               listModel = (DefaultListModel<String>) jListRoleStorage.getModel();
               break;
         }
      }
      else
      {
         return;
      }

      for (String host : data)
       {
          if (!host.isEmpty())
          {
             listModel.addElement(host);
          }
       }
   }

   private static class IBCheckBox extends javax.swing.JCheckBox
   {
      private static final long serialVersionUID = 1L;

      String node;
      IBCheckBox(String node)
      {
         super();
         this.node = node;
      }

      String getNode()
      {
         return node;
      }
   }

   private static class IBIncTextField extends javax.swing.JTextField
   {
      private static final long serialVersionUID = 1L;

      String node;
      IBIncTextField(String node)
      {
         super();
         this.node = node;
      }

      String getNode()
      {
         return node;
      }
   }

   private static class IBLibTextField extends javax.swing.JTextField
   {
      private static final long serialVersionUID = 1L;

      String node;
      IBLibTextField(String node)
      {
         super();
         this.node = node;
      }

      String getNode()
      {
         return node;
      }
   }

   private static class IBKernelIncTextField extends javax.swing.JTextField
   {
      private static final long serialVersionUID = 1L;

      String node;
      IBKernelIncTextField(String node)
      {
         super();
         this.node = node;
      }

      String getNode()
      {
         return node;
      }
   }

   private class IBOneForAllButton extends javax.swing.JButton
   {
      private static final long serialVersionUID = 1L;

      String node;
      IBOneForAllButton(String node)
      {
         super();
         this.node = node;
         this.setText(Main.getLocal().getString("Use for all"));
         this.setToolTipText(Main.getLocal().getString("Use this configuration for all nodes. Note : You still need to ")
        		 + Main.getLocal().getString("enable IB for the nodes."));
         this.addActionListener(new java.awt.event.ActionListener()
         {
            @Override
            public void actionPerformed(ActionEvent evt)
            {
               // +1 the managment node
               int maxNodeCount = metaNodes.size() + storageNodes.size() + storageNodes.size() + 1;

               ArrayList<IBIncTextField> incPathsToChange = new ArrayList<>(maxNodeCount);
               ArrayList<IBLibTextField> libPathsToChange = new ArrayList<>(maxNodeCount);
               ArrayList<IBKernelIncTextField> kincPathsToChange = new ArrayList<>(maxNodeCount);

               String incPath = "";
               String libPath = "";
               String kincPath = "";
               Component[] comps = jPanelIBConfigInner.getComponents();
               for (Component comp : comps)
               {
                  if (comp instanceof IBIncTextField)
                  {
                     IBIncTextField tf = (IBIncTextField) comp;
                     IBOneForAllButton b = (IBOneForAllButton) evt.getSource();
                     if (tf.getNode().equals(b.getNode()))
                     {
                        incPath = tf.getText();
                     }
                     else
                     {
                        incPathsToChange.add(tf);
                     }
                  }
                  if (comp instanceof IBLibTextField)
                  {
                     IBLibTextField tf = (IBLibTextField) comp;
                     IBOneForAllButton b = (IBOneForAllButton) evt.getSource();
                     if (tf.getNode().equals(b.getNode()))
                     {
                        libPath = tf.getText();
                     }
                     else
                     {
                        libPathsToChange.add(tf);
                     }
                  }
                  if (comp instanceof IBKernelIncTextField)
                  {
                     IBKernelIncTextField tf = (IBKernelIncTextField) comp;
                     IBOneForAllButton b = (IBOneForAllButton) evt.getSource();
                     if (tf.getNode().equals(b.getNode()))
                     {
                        kincPath = tf.getText();
                     }
                     else
                     {
                        kincPathsToChange.add(tf);
                     }
                  }
               }

               for (IBIncTextField tf : incPathsToChange)
               {
                  tf.setText(incPath);
               }

               for (IBLibTextField tf : libPathsToChange)
               {
                  tf.setText(libPath);
               }

               for (IBKernelIncTextField tf : kincPathsToChange)
               {
                  tf.setText(kincPath);
               }
            }
         });
      }

      String getNode()
      {
         return node;
      }
   }

   public JInternalFrameInstallationConfig()
   {
      isSavedRoles = true;
      isSavedConfig = true;
      isSavedIB = true;
      saveDefaultIBConfig = false;
      lastTab = 0;
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());

      loadRoles();
      loadConfig();
      loadIBConfig();

      isSavedRoles = true;
      isSavedConfig = true;
      isSavedIB = true;
      saveDefaultIBConfig = false;
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameInstallationConfig;
   }

   private boolean confirmNotSaved()
   {
	   int ret = JOptionPane.showConfirmDialog(this, Main.getLocal().getString("You did not save your changes! Would you ")
			   + Main.getLocal().getString("like to save now?"), Main.getLocal().getString("Save changes?"), JOptionPane.YES_NO_OPTION);
      return ret == JOptionPane.YES_OPTION;
   }

   private boolean confirmAllNotSaved()
   {
      String notSavedTabs = null;
      if (!isSavedRoles)
      {
    	  notSavedTabs = Main.getLocal().getString("Define roles");
      }
      if (!isSavedConfig)
      {
         if (notSavedTabs == null)
         {
        	 notSavedTabs = Main.getLocal().getString("Create basic configuration");
         }
         else
         {
        	 notSavedTabs += Main.getLocal().getString(", Create basic configuration");
         }
      }
      if (!isSavedIB)
      {
         if (notSavedTabs == null)
         {
        	 notSavedTabs = Main.getLocal().getString("Configure Infiniband");
         }
         else
         {
        	 notSavedTabs += Main.getLocal().getString(", Configure Infiniband");
         }
      }

      int ret = JOptionPane.showConfirmDialog(this, Main.getLocal().getString("You did not save your changes in the tabs ") +
    		  notSavedTabs + Main.getLocal().getString("! Would you like to save now?"), Main.getLocal().getString("Save changes?"),
              JOptionPane.YES_NO_OPTION);
      return ret == JOptionPane.YES_OPTION;
   }

   private void loadRoles()
   {
      try
      {
         XMLParser parser = new XMLParser(HttpTk.generateAdmonUrl("/XML_ConfigRoles"), THREAD_NAME);
         parser.update();
         mgmtdServers = parser.getVector("mgmtd");
         if (mgmtdServers.isEmpty())
         {
            mgmtdServer = "";
         }
         else
         {
            mgmtdServer = mgmtdServers.get(0);
         }
         metaNodes = parser.getVector("meta");
         storageNodes = parser.getVector("storage");
         clientNodes = parser.getVector("client");

         jTextRoleFieldMgmtd.setText(mgmtdServer);

         ListModel<String> listModelMeta = jListRoleMeta.getModel();
         DefaultListModel<String> modelMeta = (DefaultListModel<String>) listModelMeta;
         for (Iterator<String> iter = metaNodes.iterator(); iter.hasNext();)
         {
            modelMeta.addElement(iter.next());
         }

         ListModel<String> listModelStorage = jListRoleStorage.getModel();
         DefaultListModel<String> modelStorage = (DefaultListModel<String>) listModelStorage;
         for (Iterator<String> iter = storageNodes.iterator(); iter.hasNext();)
         {
            modelStorage.addElement(iter.next());
         }

         ListModel<String> listModelClient = jListRoleClient.getModel();
         DefaultListModel<String> modelClient = (DefaultListModel<String>) listModelClient;
         for (Iterator<String> iter = clientNodes.iterator(); iter.hasNext();)
         {
            modelClient.addElement(iter.next());
         }

      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
      }
   }

   private void loadConfig()
   {
      try
      {
         XMLParser parser = new XMLParser(HttpTk.generateAdmonUrl("/XML_ConfigBasicConfig"),
            THREAD_NAME);
         parser.update();
         TreeMap<String, String> config = parser.getTreeMap();
         if (config.get("storeMetaDirectory") != null)
         {
        	 jTextFieldMetaDir.setText(config.get("storeMetaDirectory"));
         }
         else
         {
            jTextFieldMetaDir.setText("");
         }
         if (config.get("connMetaPortTCP") != null)
         {
        	 jTextFieldMetaPort.setText(config.get("connMetaPortTCP"));
         }
         else
         {
            jTextFieldMetaPort.setText("");
         }
         if (config.get("storeStorageDirectory") != null)
         {
        	 jTextFieldStorageDir.setText(config.get("storeStorageDirectory"));
         }
         else
         {
            jTextFieldStorageDir.setText("");
         }
         if (config.get("connStoragePortTCP") != null)
         {
        	 jTextFieldStoragePort.setText(config.get("connStoragePortTCP"));
         }
         else
         {
            jTextFieldStoragePort.setText("");
         }
         if (config.get("connClientPortTCP") != null)
         {
        	 jTextFieldClientPort.setText(config.get("connClientPortTCP"));
         }
         else
         {
            jTextFieldClientPort.setText("");
         }
         if (config.get("clientMount") != null)
         {
        	 jTextFieldClientMount.setText(config.get("clientMount"));
         }
         else
         {
            jTextFieldClientMount.setText("");
         }
         if (config.get("connHelperdPortTCP") != null)
         {
        	 jTextFieldHelperdPort.setText(config.get("connHelperdPortTCP"));
         }
         else
         {
            jTextFieldHelperdPort.setText("");
         }
         if (config.get("storeMgmtdDirectory") != null)
         {
        	 jTextFieldMgmtdDir.setText(config.get("storeMgmtdDirectory"));
         }
         else
         {
            jTextFieldMgmtdDir.setText("");
         }

         if (config.get("connMgmtdPortTCP") != null)
         {
        	 jTextFieldMgmtdPort.setText(config.get("connMgmtdPortTCP"));
         }
         else
         {
            jTextFieldMgmtdPort.setText("");
         }
         if (config.get("tuneFileCacheType") != null)
         {
            jComboBoxClientCache.setSelectedItem(config.get("tuneFileCacheType"));
         }
         else
         {
            jComboBoxClientCache.setSelectedItem("buffered");
         }

         if (config.get("logLevel") != null)
         {
            jComboBoxLogLevel.setSelectedItem(config.get("logLevel"));
         }
         else
         {
            jComboBoxLogLevel.setSelectedItem("3");
         }

         if ((config.get("storeUseExtendedAttribs") != null) &&
                 (config.get("storeUseExtendedAttribs").equals("true")))
         {
            jComboBoxXAttr.setSelectedItem("yes");
         }
         else
         {
            jComboBoxXAttr.setSelectedItem("no");
         }

         boolean useRDMA = Boolean.parseBoolean(config.get("connUseRDMA"));
         if (useRDMA)
         {
            jCheckBoxUseRDMA.setSelected(true);
         }
         else
         {
            jCheckBoxUseRDMA.setSelected(false);
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
      }
   }

   private void loadIBConfig()
   {
      // get the nodes
      TreeSet<String> nodeIDs = new java.util.TreeSet<>();
      nodeIDs.addAll(metaNodes);
      nodeIDs.addAll(storageNodes);
      nodeIDs.addAll(clientNodes);

      TreeMap<String, String> attributes = new TreeMap<>();

      // create the layout
      jPanelIBConfigInner.removeAll();
      GroupLayout layout = new GroupLayout(jPanelIBConfigInner);
      jPanelIBConfigInner.setLayout(layout);
      layout.setAutoCreateGaps(true);
      layout.setAutoCreateContainerGaps(true);

      // Create a sequential group for the horizontal axis.

      GroupLayout.SequentialGroup hGroup = layout.createSequentialGroup();

      // Sequential group for the rows

      GroupLayout.SequentialGroup vGroup = layout.createSequentialGroup();

      // The sequential group in turn contains 4 parallel groups.
      // One parallel group contains the labels, the others the text fields.
      // Putting the labels in a parallel group positions them at the same x location.
      // ( => 4 parallel groups, one for each column)
      // Variable indentation is used to reinforce the level of grouping.

      // first column

      GroupLayout.ParallelGroup pGroup1 = layout.createParallelGroup(GroupLayout.Alignment.CENTER);
      GroupLayout.ParallelGroup pGroup2 = layout.createParallelGroup(GroupLayout.Alignment.CENTER);
      GroupLayout.ParallelGroup pGroup5 = layout.createParallelGroup(GroupLayout.Alignment.CENTER);
      GroupLayout.ParallelGroup pGroup6 = layout.createParallelGroup(GroupLayout.Alignment.CENTER);

      GroupLayout.ParallelGroup pGroupVertical = layout.createParallelGroup(Alignment.BASELINE);

      JCheckBox cb = new JCheckBox();
      cb.setText("");
      cb.setToolTipText(Main.getLocal().getString("Toggle all hosts to support (or not support) infiniband"));

      cb.addItemListener(new java.awt.event.ItemListener()
      {
         @Override
         public void itemStateChanged(ItemEvent e)
         {
            isSavedIB = false;
            boolean state = false;
            if (e.getStateChange() == ItemEvent.SELECTED)
            {
               state = true;
            }
            Component[] comps = jPanelIBConfigInner.getComponents();
            for (Component comp : comps)
            {
               if (comp instanceof IBCheckBox)
               {
                  JCheckBox c = (JCheckBox) comp;
                  c.setSelected(state);
               }
            }
         }
      });

      pGroup1.addComponent(cb);
      pGroupVertical.addComponent(cb);

      JLabel labelHeading = new JLabel(Main.getLocal().getString("<html><b><u><center>Node ID</center></u></b></html>"));
      labelHeading.setHorizontalAlignment(JLabel.CENTER);
      labelHeading.setHorizontalTextPosition(JLabel.LEADING);
      labelHeading.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.
         getResource(FilePathsEnum.IMAGE_INFO.getPath() ) ) ); // NOI18N
      pGroup2.addComponent(labelHeading);
      pGroupVertical.addComponent(labelHeading);

      labelHeading = new JLabel(Main.getLocal().getString("<html><b><u><center>IB kernel include path</center></u></b>")
         + "</html>");
      labelHeading.setHorizontalTextPosition(JLabel.LEADING);
      labelHeading.setHorizontalAlignment(JLabel.CENTER);
      labelHeading.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.
         getResource(FilePathsEnum.IMAGE_INFO.getPath()))); // NOI18N
      String tooltipText = "Path to OFED kernel include files - This path must contain a " +
         "directory called \"rdma\", which contains the file \"ib_verbs.h\"";
      labelHeading.setToolTipText(tooltipText);
      pGroup5.addComponent(labelHeading);
      pGroupVertical.addComponent(labelHeading);
      vGroup.addGroup(pGroupVertical);

      for (Iterator<String> iter = nodeIDs.iterator(); iter.hasNext();)
      {
         try
         {
            pGroupVertical = layout.createParallelGroup(Alignment.BASELINE);
            String node = iter.next();
            try
            {
               XMLParser parser = new XMLParser(HttpTk.generateAdmonUrl("/XML_ConfigIBConfig"),
                  THREAD_NAME);
               parser.update();
               attributes = parser.getTreeMap(node);
            }
            catch (java.lang.NullPointerException e)
            {
               LOGGER.log(Level.FINEST, Main.getLocal().getString("Internal error."), e);
            }

            IBCheckBox checkbox = new IBCheckBox(node);
            checkbox.setPreferredSize(new Dimension(20,20));
            checkbox.setToolTipText(Main.getLocal().getString("Toggle on if BeeGFS should try to support infiniband on this ")
                    + Main.getLocal().getString("host, toggle off if not"));
            checkbox.setSelected(true);

            if (attributes.isEmpty())
            {
               checkbox.setSelected(false);
               saveDefaultIBConfig = true;
            }
            else
            {
               checkbox.setSelected(true);
            }

            checkbox.addItemListener(new java.awt.event.ItemListener()
            {
               @Override
               public void itemStateChanged(ItemEvent e)
               {
                  isSavedIB = false;
               }
            });
            pGroup1.addComponent(checkbox);
            pGroupVertical.addComponent(checkbox);

            JLabel label = new JLabel(node);
            label.setHorizontalAlignment(JLabel.CENTER);
            pGroup2.addComponent(label);
            pGroupVertical.addComponent(label);

            IBKernelIncTextField kIncTF = new IBKernelIncTextField(node);
            kIncTF.addKeyListener(new java.awt.event.KeyListener()
            {
               @Override
               public void keyTyped(KeyEvent e)
               {
                  isSavedIB = false;
               }

               @Override
               public void keyPressed(KeyEvent e)
               {
               }

               @Override
               public void keyReleased(KeyEvent e)
               {
               }
            });

            try
            {
               kIncTF.setText(attributes.get("kernelIncPath"));
            }
            catch (java.lang.NullPointerException e)
            {
               LOGGER.log(Level.FINEST, Main.getLocal().getString("Internal error."), e);
            }
            pGroup5.addComponent(kIncTF);
            pGroupVertical.addComponent(kIncTF);

            IBOneForAllButton oneForAllButton = new IBOneForAllButton(node);
            oneForAllButton.setPreferredSize(new Dimension(100,20));
            pGroup6.addComponent(oneForAllButton);
            pGroupVertical.addComponent(oneForAllButton);

            vGroup.addGroup(pGroupVertical);
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
         }
      }
      hGroup.addGroup(pGroup1);
      hGroup.addGroup(pGroup2);
      hGroup.addGroup(pGroup5);
      hGroup.addGroup(pGroup6);

      layout.setHorizontalGroup(hGroup);
      layout.setVerticalGroup(vGroup);

   }

   private boolean uploadIBConfig()
   {
      int maxNodeCount = this.metaNodes.size() + this.storageNodes.size() +
         this.storageNodes.size() + 1; // +1 the managment node

      InputStream serverResp = null;
      ArrayList<String> ibNodes = new ArrayList<>(maxNodeCount);
      TreeMap<String, String> kernelIncPaths = new TreeMap<>();

      for (Component comp : jPanelIBConfigInner.getComponents())
      {
         if (comp instanceof IBCheckBox)
         {
            IBCheckBox c = (IBCheckBox) comp;
            if (c.isSelected())
            {
               ibNodes.add(c.getNode());
            }
         }
         else if (comp instanceof IBKernelIncTextField)
         {
            IBKernelIncTextField tf = (IBKernelIncTextField) comp;
            kernelIncPaths.put(tf.getNode(), tf.getText());
         }
      }
      try
      {
         String url = HttpTk.generateAdmonUrl("/XML_ConfigIBConfig");
         int dataSize = ibNodes.size();
         int objSize = (2 * dataSize) + 2;
         Object[] obj = new Object[objSize];
         obj[0] = "saveIBConfig";
         obj[1] = "Save";
         int objCounter = 2;
         for (String node : ibNodes)
         {
            obj[objCounter] = node + "_kernelibincpath";
            objCounter++;
            obj[objCounter] = kernelIncPaths.get(node);
            objCounter++;
         }

         serverResp = HttpTk.sendPostData(url, obj);

         XMLParser parser = new XMLParser(THREAD_NAME);
         parser.readFromInputStream(serverResp);
         ArrayList<String> errors = parser.getVector("errors");

         if (errors.isEmpty())
         {
            if (!saveDefaultIBConfig)
            {
            	JOptionPane.showMessageDialog(null, Main.getLocal().getString("IB configuration successfully written"),
            	Main.getLocal().getString("Success"), JOptionPane.INFORMATION_MESSAGE);
            }

            return true;
         }
         else
         {
            StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
            for (String error : errors)
            {
               errorStr.append(error);
               errorStr.append(System.lineSeparator());
            }
            JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                    JOptionPane.ERROR_MESSAGE);
            return false;
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
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
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("IO Error occured"), e);
         }
      }
      return false;
   }

   private boolean uploadRoles()
   {
      InputStream serverResp = null;
      try
      {
         String url = HttpTk.generateAdmonUrl("/XML_ConfigRoles");
         int dataSize = jListRoleMeta.getModel().getSize() + jListRoleStorage.getModel().getSize() +
                 jListRoleClient.getModel().getSize();
         int objSize = (2 * dataSize) + 4;
         Object[] obj = new Object[objSize];
         int i;
         obj[0] = "saveRoles";
         obj[1] = "true";
         obj[2] = "mgmtd";
         obj[3] = jTextRoleFieldMgmtd.getText();
         mgmtdServer = jTextRoleFieldMgmtd.getText();
         int objCounter = 4;

         for (i = 0; i < jListRoleMeta.getModel().getSize(); i++)
         {
            obj[objCounter] = "meta";
            objCounter++;
            String node = jListRoleMeta.getModel().getElementAt(i);
            obj[objCounter] = node;
            objCounter++;
            metaNodes.add(node);
         }

         for (i = 0; i < jListRoleStorage.getModel().getSize(); i++)
         {
            obj[objCounter] = "storage";
            objCounter++;
            String node = jListRoleStorage.getModel().getElementAt(i);
            obj[objCounter] = node;
            objCounter++;
            storageNodes.add(node);
         }

         for (i = 0; i < jListRoleClient.getModel().getSize(); i++)
         {
            obj[objCounter] = "client";
            objCounter++;
            String node = jListRoleClient.getModel().getElementAt(i);
            obj[objCounter] = node;
            objCounter++;
            clientNodes.add(node);
         }

         serverResp = HttpTk.sendPostData(url, obj);
         XMLParser parser = new XMLParser(THREAD_NAME);
         parser.readFromInputStream(serverResp);
         ArrayList<String> errors = parser.getVector("errors");
         if (errors.isEmpty())
         {
            return true;
         }
         else
         {
            StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
            for (String error : errors)
            {
               errorStr.append(error);
               errorStr.append(System.lineSeparator());
            }
            JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                    JOptionPane.ERROR_MESSAGE);
            return false;
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
      }
      catch (HeadlessException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Internal error."), new Object[]{e, true});
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
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("IO error"), e);
         }
      }
      return false;
   }

   private boolean uploadConfig()
   {
      InputStream serverResp = null;
      try
      {
         String url = HttpTk.generateAdmonUrl("/XML_ConfigBasicConfig");
         boolean useXAttr = true;
         if (jComboBoxXAttr.getSelectedItem().toString().equals("no"))
         {
            useXAttr = false;
         }

         Object[] obj = new Object[]
         {
            "saveConfig", "Save",
            "storeMetaDirectory", jTextFieldMetaDir.getText(),
            "connMetaPortUDP", jTextFieldMetaPort.getText(),
            "connMetaPortTCP", jTextFieldMetaPort.getText(),
            "storeUseExtendedAttribs", String.valueOf(useXAttr),
            "storeStorageDirectory", jTextFieldStorageDir.getText(),
            "connStoragePortUDP", jTextFieldStoragePort.getText(),
            "connStoragePortTCP", jTextFieldStoragePort.getText(),
            "connClientPortUDP", jTextFieldClientPort.getText(),
            "connClientPortTCP", jTextFieldClientPort.getText(),
            "clientMount", jTextFieldClientMount.getText(),
            "storeMgmtdDirectory", jTextFieldMgmtdDir.getText(),
            "connMgmtdPortUDP", jTextFieldMgmtdPort.getText(),
            "connMgmtdPortTCP", jTextFieldMgmtdPort.getText(),
            "connHelperdPortTCP", jTextFieldHelperdPort.getText(),
            "connUseSDP", "false",
            "connUseRDMA", String.valueOf(jCheckBoxUseRDMA.isSelected()),
            "logLevel", String.valueOf(jComboBoxLogLevel.getSelectedItem().toString()),
            "tuneFileCacheType", String.valueOf(jComboBoxClientCache.getSelectedItem().toString())
         };

         serverResp = HttpTk.sendPostData(url, obj);
         XMLParser parser = new XMLParser(THREAD_NAME);
         parser.readFromInputStream(serverResp);
         ArrayList<String> errors = parser.getVector("errors");
         if (errors.isEmpty())
         {
            JOptionPane.showMessageDialog(null, Main.getLocal().getString("Basic configuration successfully written"),
                    Main.getLocal().getString("Success"), JOptionPane.INFORMATION_MESSAGE);
            return true;
         }
         else
         {
            StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
            for (String error : errors)
            {
               errorStr.append(error);
               errorStr.append(System.lineSeparator());
            }
            JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                    JOptionPane.ERROR_MESSAGE);
            return false;
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
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
      return false;
   }

   private boolean checkDistriArch()
   {
      boolean finished = false;
      XMLParser parser = new XMLParser(HttpTk.generateAdmonUrl("/XML_CheckDistriArch"), true,
         THREAD_NAME);
      parser.start();

      try
      {
         //wait until the parser thread ends
         parser.join(0);
         finished = Boolean.parseBoolean(parser.getValue("finished"));
      }
      catch (InterruptedException | CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Couldn't parse XML-data from the admon!"), new Object[]{e, true});
      }
      return finished;
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

      jPopupMenuMeta = new javax.swing.JPopupMenu();
      jMenuItemAddMeta = new javax.swing.JMenuItem();
      jMenuItemImportHostListMeta = new javax.swing.JMenuItem();
      jMenuItemDeleteSelectedMeta = new javax.swing.JMenuItem();
      jPopupMenuStorage = new javax.swing.JPopupMenu();
      jMenuItemAddStorage = new javax.swing.JMenuItem();
      jMenuItemImportHostListStorage = new javax.swing.JMenuItem();
      jMenuItemDeleteSelectedStorage = new javax.swing.JMenuItem();
      jPopupMenuClient = new javax.swing.JPopupMenu();
      jMenuItemAddClient = new javax.swing.JMenuItem();
      jMenuItemImportHostListClient = new javax.swing.JMenuItem();
      jMenuItemDeleteSelectedClient = new javax.swing.JMenuItem();
      jScrollPane10 = new javax.swing.JScrollPane();
      jPanelFrame = new javax.swing.JPanel();
      jTabbedPaneTabs = new javax.swing.JTabbedPane();
      jPanelRolesConf = new javax.swing.JPanel();
      jPanelRolesHeader = new javax.swing.JPanel();
      jTextAreaRolesDescription = new javax.swing.JTextArea();
      jTextRoleAreaHintRightClick = new javax.swing.JTextArea();
      jPanelRoleMgmtd = new javax.swing.JPanel();
      jTextRoleFieldMgmtd = new javax.swing.JTextField();
      jLabelRoleMgmtd = new javax.swing.JLabel();
      jPanelRoleLists = new javax.swing.JPanel();
      jScrollPaneRoleMeta = new javax.swing.JScrollPane();
      jListRoleMeta = new javax.swing.JList<>();
      filler2 = new javax.swing.Box.Filler(new java.awt.Dimension(10, 10), new java.awt.Dimension(10, 10), new java.awt.Dimension(10, 10));
      jScrollPaneRoleStorage = new javax.swing.JScrollPane();
      jListRoleStorage = new javax.swing.JList<>();
      filler1 = new javax.swing.Box.Filler(new java.awt.Dimension(10, 10), new java.awt.Dimension(10, 10), new java.awt.Dimension(10, 10));
      jScrollPaneRoleClient = new javax.swing.JScrollPane();
      jListRoleClient = new javax.swing.JList<>();
      jPanelBasicConf = new javax.swing.JPanel();
      jTextAreaBasicConfDescription = new javax.swing.JTextArea();
      jScrollPaneBasicConf = new javax.swing.JScrollPane();
      jPanelBasicConfInner = new javax.swing.JPanel();
      jLabelMetaDir = new javax.swing.JLabel();
      jTextFieldMetaDir = new javax.swing.JTextField();
      jLabelInfoMetaDir = new javax.swing.JLabel();
      jLabelMetaPort = new javax.swing.JLabel();
      jTextFieldMetaPort = new javax.swing.JTextField();
      jLabelInfoMetaPort = new javax.swing.JLabel();
      jLabelInfoStoragePort = new javax.swing.JLabel();
      jTextFieldStoragePort = new javax.swing.JTextField();
      jLabelStoragePort = new javax.swing.JLabel();
      jLabelStorageDir = new javax.swing.JLabel();
      jTextFieldStorageDir = new javax.swing.JTextField();
      jLabelInfoStorageDir = new javax.swing.JLabel();
      jLabelInfoClientMount = new javax.swing.JLabel();
      jTextFieldClientMount = new javax.swing.JTextField();
      jLabelClientMount = new javax.swing.JLabel();
      jLabelClientPort = new javax.swing.JLabel();
      jTextFieldClientPort = new javax.swing.JTextField();
      jLabelInfoClientPort = new javax.swing.JLabel();
      jLabelInfoMgmtdPort = new javax.swing.JLabel();
      jTextFieldMgmtdPort = new javax.swing.JTextField();
      jLabelMgmtdPort = new javax.swing.JLabel();
      jLabelMgmtdDir = new javax.swing.JLabel();
      jLabelHelperdPort = new javax.swing.JLabel();
      jTextFieldHelperdPort = new javax.swing.JTextField();
      jTextFieldMgmtdDir = new javax.swing.JTextField();
      jLabelInfoHelperdPort = new javax.swing.JLabel();
      jLabelInfoMgmtdDir = new javax.swing.JLabel();
      jLabelUseRDMA = new javax.swing.JLabel();
      jLabelInfoUseRDMA = new javax.swing.JLabel();
      jCheckBoxUseRDMA = new javax.swing.JCheckBox();
      jLabelLogLevel = new javax.swing.JLabel();
      jLabelInfoLogLevel = new javax.swing.JLabel();
      jLabelClientCache = new javax.swing.JLabel();
      jLabelInfoClientCache = new javax.swing.JLabel();
      jComboBoxClientCache = new javax.swing.JComboBox<>();
      jComboBoxLogLevel = new javax.swing.JComboBox<>();
      jLabelXAttr = new javax.swing.JLabel();
      jComboBoxXAttr = new javax.swing.JComboBox<>();
      jLabelInfoXAttr = new javax.swing.JLabel();
      jPanelIBConf = new javax.swing.JPanel();
      jTextAreaIBConfDescription = new javax.swing.JTextArea();
      jScrollPaneIBConfig = new javax.swing.JScrollPane();
      jPanelIBConfigInner = new javax.swing.JPanel();
      jPanelButtons = new javax.swing.JPanel();
      jButtonSave = new javax.swing.JButton();
      jButtonReload = new javax.swing.JButton();
      fillerButtons = new javax.swing.Box.Filler(new java.awt.Dimension(300, 0), new java.awt.Dimension(300, 0), new java.awt.Dimension(32767, 32767));
      jButtonNext = new javax.swing.JButton();

      jPopupMenuMeta.setInvoker(jListRoleMeta);

      jMenuItemAddMeta.setText(Main.getLocal().getString("Add host"));
      jMenuItemAddMeta.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemAddMetaActionPerformed(evt);
         }
      });
      jPopupMenuMeta.add(jMenuItemAddMeta);

      jMenuItemImportHostListMeta.setText(Main.getLocal().getString("Import hosts from file"));
      jMenuItemImportHostListMeta.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemImportHostListMetaActionPerformed(evt);
         }
      });
      jPopupMenuMeta.add(jMenuItemImportHostListMeta);

      jMenuItemDeleteSelectedMeta.setText(Main.getLocal().getString("Delete selected"));
      jMenuItemDeleteSelectedMeta.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemDeleteSelectedMetaActionPerformed(evt);
         }
      });
      jPopupMenuMeta.add(jMenuItemDeleteSelectedMeta);

      jPopupMenuStorage.setInvoker(jListRoleMeta);

      jMenuItemAddStorage.setText(Main.getLocal().getString("Add host"));
      jMenuItemAddStorage.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemAddStorageActionPerformed(evt);
         }
      });
      jPopupMenuStorage.add(jMenuItemAddStorage);

      jMenuItemImportHostListStorage.setText(Main.getLocal().getString("Import hosts from file"));
      jMenuItemImportHostListStorage.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemImportHostListStorageActionPerformed(evt);
         }
      });
      jPopupMenuStorage.add(jMenuItemImportHostListStorage);

      jMenuItemDeleteSelectedStorage.setText(Main.getLocal().getString("Delete selected"));
      jMenuItemDeleteSelectedStorage.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemDeleteSelectedStorageActionPerformed(evt);
         }
      });
      jPopupMenuStorage.add(jMenuItemDeleteSelectedStorage);

      jPopupMenuClient.setInvoker(jListRoleMeta);

      jMenuItemAddClient.setText(Main.getLocal().getString("Add host"));
      jMenuItemAddClient.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemAddClientActionPerformed(evt);
         }
      });
      jPopupMenuClient.add(jMenuItemAddClient);

      jMenuItemImportHostListClient.setText(Main.getLocal().getString("Import hosts from file"));
      jMenuItemImportHostListClient.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemImportHostListClientActionPerformed(evt);
         }
      });
      jPopupMenuClient.add(jMenuItemImportHostListClient);

      jMenuItemDeleteSelectedClient.setText(Main.getLocal().getString("Delete selected"));
      jMenuItemDeleteSelectedClient.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jMenuItemDeleteSelectedClientActionPerformed(evt);
         }
      });
      jPopupMenuClient.add(jMenuItemDeleteSelectedClient);

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setPreferredSize(new java.awt.Dimension(915, 715));
      addInternalFrameListener(new javax.swing.event.InternalFrameListener()
      {
         public void internalFrameOpened(javax.swing.event.InternalFrameEvent evt)
         {
            formInternalFrameOpened(evt);
         }
         public void internalFrameClosing(javax.swing.event.InternalFrameEvent evt)
         {
            formInternalFrameClosing(evt);
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

      jScrollPane10.setPreferredSize(new java.awt.Dimension(900, 700));

      jPanelFrame.setPreferredSize(new java.awt.Dimension(890, 680));
      jPanelFrame.setLayout(new java.awt.BorderLayout());

      jTabbedPaneTabs.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jTabbedPaneTabs.setPreferredSize(new java.awt.Dimension(818, 645));
      jTabbedPaneTabs.addChangeListener(new javax.swing.event.ChangeListener()
      {
         public void stateChanged(javax.swing.event.ChangeEvent evt)
         {
            jTabbedPaneTabsStateChanged(evt);
         }
      });

      jPanelRolesConf.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelRolesConf.setLayout(new java.awt.BorderLayout(10, 10));

      jPanelRolesHeader.setLayout(new java.awt.BorderLayout(5, 5));

      jTextAreaRolesDescription.setEditable(false);
      jTextAreaRolesDescription.setBackground(new java.awt.Color(238, 238, 238));
      jTextAreaRolesDescription.setColumns(20);
      jTextAreaRolesDescription.setLineWrap(true);
      jTextAreaRolesDescription.setRows(5);
      jTextAreaRolesDescription.setText(Main.getLocal().getString("Step 1 : Define roles,\n\nPlease define the management host and the names of the hosts that shall act as metadata servers, storage servers and clients. For each category provide one hostname per line. The default value for the management daemon is the same host, which runs the admon daemon."));
      jTextAreaRolesDescription.setWrapStyleWord(true);
      jTextAreaRolesDescription.setBorder(null);
      jTextAreaRolesDescription.setMargin(new java.awt.Insets(10, 10, 10, 10));
      jPanelRolesHeader.add(jTextAreaRolesDescription, java.awt.BorderLayout.NORTH);

      jTextRoleAreaHintRightClick.setEditable(false);
      jTextRoleAreaHintRightClick.setBackground(new java.awt.Color(238, 238, 238));
      jTextRoleAreaHintRightClick.setColumns(20);
      jTextRoleAreaHintRightClick.setFont(new java.awt.Font("Dialog", 1, 12)); // NOI18N
      jTextRoleAreaHintRightClick.setLineWrap(true);
      jTextRoleAreaHintRightClick.setRows(1);
      jTextRoleAreaHintRightClick.setText(Main.getLocal().getString("Right-Click into the boxes to modify the lists."));
      jTextRoleAreaHintRightClick.setWrapStyleWord(true);
      jTextRoleAreaHintRightClick.setBorder(null);
      jPanelRolesHeader.add(jTextRoleAreaHintRightClick, java.awt.BorderLayout.CENTER);

      jPanelRoleMgmtd.setLayout(new java.awt.BorderLayout());

      jTextRoleFieldMgmtd.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jTextRoleFieldMgmtdActionPerformed(evt);
         }
      });
      jTextRoleFieldMgmtd.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextRoleFieldMgmtdKeyTyped(evt);
         }
      });
      jPanelRoleMgmtd.add(jTextRoleFieldMgmtd, java.awt.BorderLayout.CENTER);

      jLabelRoleMgmtd.setText(Main.getLocal().getString("Management daemon :"));
      jPanelRoleMgmtd.add(jLabelRoleMgmtd, java.awt.BorderLayout.LINE_START);

      jPanelRolesHeader.add(jPanelRoleMgmtd, java.awt.BorderLayout.SOUTH);

      jPanelRolesConf.add(jPanelRolesHeader, java.awt.BorderLayout.NORTH);

      jPanelRoleLists.setLayout(new javax.swing.BoxLayout(jPanelRoleLists, javax.swing.BoxLayout.LINE_AXIS));

      jScrollPaneRoleMeta.setBorder(null);

      jListRoleMeta.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Metadata servers")));
      jListRoleMeta.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_INTERVAL_SELECTION);
      jListRoleMeta.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mousePressed(java.awt.event.MouseEvent evt)
         {
            jListRoleMetaMousePressed(evt);
         }
         public void mouseReleased(java.awt.event.MouseEvent evt)
         {
            jListRoleMetaMouseReleased(evt);
         }
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jListRoleMetaMouseClicked(evt);
         }
      });
      jScrollPaneRoleMeta.setViewportView(jListRoleMeta);
      jListRoleMeta.setModel(new DefaultListModel<String>());

      jPanelRoleLists.add(jScrollPaneRoleMeta);
      jPanelRoleLists.add(filler2);

      jScrollPaneRoleStorage.setBorder(null);

      jListRoleStorage.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Storage servers")));
      jListRoleStorage.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_INTERVAL_SELECTION);
      jListRoleStorage.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mousePressed(java.awt.event.MouseEvent evt)
         {
            jListRoleStorageMousePressed(evt);
         }
         public void mouseReleased(java.awt.event.MouseEvent evt)
         {
            jListRoleStorageMouseReleased(evt);
         }
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jListRoleStorageMouseClicked(evt);
         }
      });
      jScrollPaneRoleStorage.setViewportView(jListRoleStorage);
      jListRoleStorage.setModel(new DefaultListModel<String>());

      jPanelRoleLists.add(jScrollPaneRoleStorage);
      jPanelRoleLists.add(filler1);

      jScrollPaneRoleClient.setBorder(null);

      jListRoleClient.setBorder(javax.swing.BorderFactory.createTitledBorder(Main.getLocal().getString("Clients")));
      jListRoleClient.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_INTERVAL_SELECTION);
      jListRoleClient.addMouseListener(new java.awt.event.MouseAdapter()
      {
         public void mousePressed(java.awt.event.MouseEvent evt)
         {
            jListRoleClientMousePressed(evt);
         }
         public void mouseReleased(java.awt.event.MouseEvent evt)
         {
            jListRoleClientMouseReleased(evt);
         }
         public void mouseClicked(java.awt.event.MouseEvent evt)
         {
            jListRoleClientMouseClicked(evt);
         }
      });
      jScrollPaneRoleClient.setViewportView(jListRoleClient);
      jListRoleClient.setModel(new DefaultListModel<String>());

      jPanelRoleLists.add(jScrollPaneRoleClient);

      jPanelRolesConf.add(jPanelRoleLists, java.awt.BorderLayout.CENTER);

      jTabbedPaneTabs.addTab(Main.getLocal().getString("Define roles"), jPanelRolesConf);

      jPanelBasicConf.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelBasicConf.setPreferredSize(new java.awt.Dimension(813, 585));
      jPanelBasicConf.setLayout(new java.awt.BorderLayout(10, 10));

      jTextAreaBasicConfDescription.setEditable(false);
      jTextAreaBasicConfDescription.setBackground(new java.awt.Color(238, 238, 238));
      jTextAreaBasicConfDescription.setColumns(20);
      jTextAreaBasicConfDescription.setLineWrap(true);
      jTextAreaBasicConfDescription.setRows(4);
      jTextAreaBasicConfDescription.setText(Main.getLocal().getString("Step 2 : Create basic configuration\n\nThis page will create a master configuration for all of your nodes, according to the data you provide here. The default values are recommendations."));
      jTextAreaBasicConfDescription.setWrapStyleWord(true);
      jTextAreaBasicConfDescription.setBorder(null);
      jTextAreaBasicConfDescription.setMinimumSize(new java.awt.Dimension(100, 20));
      jPanelBasicConf.add(jTextAreaBasicConfDescription, java.awt.BorderLayout.NORTH);

      jScrollPaneBasicConf.setPreferredSize(new java.awt.Dimension(220, 203));

      jPanelBasicConfInner.setBorder(javax.swing.BorderFactory.createLineBorder(new java.awt.Color(0, 0, 0)));
      jPanelBasicConfInner.setMinimumSize(new java.awt.Dimension(80, 80));
      jPanelBasicConfInner.setPreferredSize(new java.awt.Dimension(220, 200));
      jPanelBasicConfInner.setLayout(new java.awt.GridBagLayout());

      jLabelMetaDir.setText(Main.getLocal().getString("Metadata directory :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelMetaDir, gridBagConstraints);

      jTextFieldMetaDir.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldMetaDir.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldMetaDir.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldMetaDirKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldMetaDir, gridBagConstraints);

      jLabelInfoMetaDir.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoMetaDir.setToolTipText(Main.getLocal().getString("The directory in which the metadata servers store the data on their local devices"));
      jLabelInfoMetaDir.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMetaDir.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMetaDir.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoMetaDir, gridBagConstraints);

      jLabelMetaPort.setText(Main.getLocal().getString("Metadata server port :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 9;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelMetaPort, gridBagConstraints);

      jTextFieldMetaPort.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldMetaPort.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldMetaPort.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldMetaPortKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 9;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldMetaPort, gridBagConstraints);

      jLabelInfoMetaPort.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoMetaPort.setToolTipText(Main.getLocal().getString("The port number (TCP and UDP) on which the meta servers will listen for connections"));
      jLabelInfoMetaPort.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMetaPort.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMetaPort.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 9;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoMetaPort, gridBagConstraints);

      jLabelInfoStoragePort.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoStoragePort.setToolTipText(Main.getLocal().getString("The port number (TCP and UDP) on which the storage servers will listen for connections"));
      jLabelInfoStoragePort.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoStoragePort.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoStoragePort.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 10;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoStoragePort, gridBagConstraints);

      jTextFieldStoragePort.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldStoragePort.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldStoragePort.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldStoragePortKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 10;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldStoragePort, gridBagConstraints);

      jLabelStoragePort.setText(Main.getLocal().getString("Storage server port :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 10;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelStoragePort, gridBagConstraints);

      jLabelStorageDir.setText(Main.getLocal().getString("Storage directory :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 2;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelStorageDir, gridBagConstraints);

      jTextFieldStorageDir.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldStorageDir.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldStorageDir.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldStorageDirKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 2;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldStorageDir, gridBagConstraints);

      jLabelInfoStorageDir.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoStorageDir.setToolTipText(Main.getLocal().getString("The directory in which the storage servers store the data on their local devices"));
      jLabelInfoStorageDir.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoStorageDir.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoStorageDir.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 2;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoStorageDir, gridBagConstraints);

      jLabelInfoClientMount.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoClientMount.setToolTipText(Main.getLocal().getString("The directory where BeeGFS will be mounted on the clients"));
      jLabelInfoClientMount.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoClientMount.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoClientMount.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 3;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoClientMount, gridBagConstraints);

      jTextFieldClientMount.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldClientMount.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldClientMount.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldClientMountKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 3;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldClientMount, gridBagConstraints);

      jLabelClientMount.setText(Main.getLocal().getString("Client mount point :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 3;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelClientMount, gridBagConstraints);

      jLabelClientPort.setText(Main.getLocal().getString("Client port :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 11;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelClientPort, gridBagConstraints);

      jTextFieldClientPort.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldClientPort.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldClientPort.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldClientPortKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 11;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldClientPort, gridBagConstraints);

      jLabelInfoClientPort.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoClientPort.setToolTipText(Main.getLocal().getString("The port number (TCP and UDP) on which the clients will listen for connections"));
      jLabelInfoClientPort.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoClientPort.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoClientPort.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 11;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoClientPort, gridBagConstraints);

      jLabelInfoMgmtdPort.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoMgmtdPort.setToolTipText(Main.getLocal().getString("The port number (TCP and UDP) on which the management daemon will listen for connections"));
      jLabelInfoMgmtdPort.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMgmtdPort.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMgmtdPort.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 8;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoMgmtdPort, gridBagConstraints);

      jTextFieldMgmtdPort.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldMgmtdPort.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldMgmtdPort.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldMgmtdPortKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 8;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldMgmtdPort, gridBagConstraints);

      jLabelMgmtdPort.setText(Main.getLocal().getString("Management daemon port :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 8;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelMgmtdPort, gridBagConstraints);

      jLabelMgmtdDir.setText(Main.getLocal().getString("Management daemon directory :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelMgmtdDir, gridBagConstraints);

      jLabelHelperdPort.setText(Main.getLocal().getString("Helper daemon port :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 12;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelHelperdPort, gridBagConstraints);

      jTextFieldHelperdPort.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldHelperdPort.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldHelperdPort.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldHelperdPortKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 12;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldHelperdPort, gridBagConstraints);

      jTextFieldMgmtdDir.setMinimumSize(new java.awt.Dimension(477, 28));
      jTextFieldMgmtdDir.setPreferredSize(new java.awt.Dimension(477, 28));
      jTextFieldMgmtdDir.addKeyListener(new java.awt.event.KeyAdapter()
      {
         public void keyTyped(java.awt.event.KeyEvent evt)
         {
            jTextFieldMgmtdDirKeyTyped(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jTextFieldMgmtdDir, gridBagConstraints);

      jLabelInfoHelperdPort.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoHelperdPort.setToolTipText(Main.getLocal().getString("The port number on which the helper daemon will listen for connections"));
      jLabelInfoHelperdPort.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoHelperdPort.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoHelperdPort.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 12;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoHelperdPort, gridBagConstraints);

      jLabelInfoMgmtdDir.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoMgmtdDir.setToolTipText(Main.getLocal().getString("The directory in which the management daemon stores its data on the local device"));
      jLabelInfoMgmtdDir.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMgmtdDir.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoMgmtdDir.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoMgmtdDir, gridBagConstraints);
      jLabelInfoMgmtdDir.getAccessibleContext().setAccessibleDescription(Main.getLocal().getString("The directory in which the management daemon stores its data on the local device. Note : The BeeGFS management daemon will automatically be installed on the setup node."));

      jLabelUseRDMA.setText(Main.getLocal().getString("Use RDMA when possible :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 4;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelUseRDMA, gridBagConstraints);

      jLabelInfoUseRDMA.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoUseRDMA.setToolTipText(Main.getLocal().getString("If activated, hosts supporting it will use native Infiniband support with RDMA (Remote Direct Memory Access)"));
      jLabelInfoUseRDMA.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoUseRDMA.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoUseRDMA.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 4;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoUseRDMA, gridBagConstraints);

      jCheckBoxUseRDMA.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
      jCheckBoxUseRDMA.setHorizontalTextPosition(javax.swing.SwingConstants.CENTER);
      jCheckBoxUseRDMA.setMaximumSize(new java.awt.Dimension(22, 28));
      jCheckBoxUseRDMA.setMinimumSize(new java.awt.Dimension(22, 28));
      jCheckBoxUseRDMA.setPreferredSize(new java.awt.Dimension(22, 28));
      jCheckBoxUseRDMA.addItemListener(new java.awt.event.ItemListener()
      {
         public void itemStateChanged(java.awt.event.ItemEvent evt)
         {
            jCheckBoxUseRDMAItemStateChanged(evt);
         }
      });
      jCheckBoxUseRDMA.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jCheckBoxUseRDMAActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 4;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jCheckBoxUseRDMA, gridBagConstraints);

      jLabelLogLevel.setText(Main.getLocal().getString("Log Level :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 7;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelLogLevel, gridBagConstraints);

      jLabelInfoLogLevel.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoLogLevel.setToolTipText("<html>Defines the amount and detail of output messages.<br>\nNote : Levels above 3 might decrease performance and<br>shall be used for debugging only<br>\nDefault : 3");
      jLabelInfoLogLevel.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoLogLevel.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoLogLevel.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 7;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoLogLevel, gridBagConstraints);

      jLabelClientCache.setText(Main.getLocal().getString("Client cache type :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 6;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelClientCache, gridBagConstraints);

      jLabelInfoClientCache.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoClientCache.setToolTipText("<html>Activates the file cache<br>Values:<br>\"none\" (direct reads and writes),<br>\"buffered\" (use a pool of small  static buffers for write-back and read-ahead),<br>\"paged\" (experimental, use the kernel  pagecache).<br> Note: When client and servers are running on the same machine, 'paged'<br>contains the typical potential for memory allocation deadlocks (also<br>known from local NFS server mounts) => so do not use 'paged' in this case<br>Default: buffered</html>");
      jLabelInfoClientCache.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoClientCache.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoClientCache.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 6;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoClientCache, gridBagConstraints);

      jComboBoxClientCache.setBackground(new java.awt.Color(255, 255, 255));
      jComboBoxClientCache.setModel(new javax.swing.DefaultComboBoxModel<>(new String[] { "none", "buffered", "paged" }));
      jComboBoxClientCache.setSelectedItem("buffered");
      jComboBoxClientCache.setMinimumSize(new java.awt.Dimension(150, 24));
      jComboBoxClientCache.setPreferredSize(new java.awt.Dimension(150, 24));
      jComboBoxClientCache.addItemListener(new java.awt.event.ItemListener()
      {
         public void itemStateChanged(java.awt.event.ItemEvent evt)
         {
            jComboBoxClientCacheItemStateChanged(evt);
         }
      });
      jComboBoxClientCache.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jComboBoxClientCacheActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 6;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jComboBoxClientCache, gridBagConstraints);

      jComboBoxLogLevel.setBackground(new java.awt.Color(255, 255, 255));
      jComboBoxLogLevel.setModel(new javax.swing.DefaultComboBoxModel<>(new String[] { "1", "2", "3", "4", "5" }));
      jComboBoxLogLevel.setSelectedItem("2");
      jComboBoxLogLevel.setMinimumSize(new java.awt.Dimension(150, 24));
      jComboBoxLogLevel.setPreferredSize(new java.awt.Dimension(150, 24));
      jComboBoxLogLevel.addItemListener(new java.awt.event.ItemListener()
      {
         public void itemStateChanged(java.awt.event.ItemEvent evt)
         {
            jComboBoxLogLevelItemStateChanged(evt);
         }
      });
      jComboBoxLogLevel.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jComboBoxLogLevelActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 7;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jComboBoxLogLevel, gridBagConstraints);

      jLabelXAttr.setText(Main.getLocal().getString("Use extended attributes for metadata :"));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 5;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelXAttr, gridBagConstraints);

      jComboBoxXAttr.setBackground(new java.awt.Color(255, 255, 255));
      jComboBoxXAttr.setModel(new javax.swing.DefaultComboBoxModel<>(new String[] { "yes", "no" }));
      jComboBoxXAttr.setSelectedItem("2");
      jComboBoxXAttr.setMinimumSize(new java.awt.Dimension(150, 24));
      jComboBoxXAttr.setPreferredSize(new java.awt.Dimension(150, 24));
      jComboBoxXAttr.addItemListener(new java.awt.event.ItemListener()
      {
         public void itemStateChanged(java.awt.event.ItemEvent evt)
         {
            jComboBoxXAttrItemStateChanged(evt);
         }
      });
      jComboBoxXAttr.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jComboBoxXAttrActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 5;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jComboBoxXAttr, gridBagConstraints);

      jLabelInfoXAttr.setIcon(new javax.swing.ImageIcon(JInternalFrameInstallationConfig.class.getResource(IMAGE_INFO.getPath())));
      jLabelInfoXAttr.setToolTipText("<html>Controls whether metadata is stored as normal file contents (=no) or as extended attributes (=yes). <br>\nDepending on the type and version of your underlying local file system, extended attributes typically are significantly faster.<br>\nNote: This setting can only be configured at first startup and cannot be changed afterwards.</html>"); // NOI18N
      jLabelInfoXAttr.setMaximumSize(new java.awt.Dimension(25, 25));
      jLabelInfoXAttr.setMinimumSize(new java.awt.Dimension(25, 25));
      jLabelInfoXAttr.setPreferredSize(new java.awt.Dimension(25, 25));
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 5;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelBasicConfInner.add(jLabelInfoXAttr, gridBagConstraints);

      jScrollPaneBasicConf.setViewportView(jPanelBasicConfInner);

      jPanelBasicConf.add(jScrollPaneBasicConf, java.awt.BorderLayout.CENTER);

      jTabbedPaneTabs.addTab(Main.getLocal().getString("Create basic configuration"), jPanelBasicConf);

      jPanelIBConf.setLayout(new java.awt.BorderLayout(10, 10));

      jTextAreaIBConfDescription.setEditable(false);
      jTextAreaIBConfDescription.setBackground(new java.awt.Color(238, 238, 238));
      jTextAreaIBConfDescription.setColumns(20);
      jTextAreaIBConfDescription.setLineWrap(true);
      jTextAreaIBConfDescription.setRows(4);
      jTextAreaIBConfDescription.setText(Main.getLocal().getString("Step 3 : Configure Infiniband\n\nIn order to use Infiniband, BeeGFS needs to know which nodes have an Infiniband interface. Furthermore some information about the location of certain files is needed. If you do not want to use Infiniband you can skip this step.\nIf your include and library files are installed in default paths (/usr/include, etc.) you can leave the corresponding fields blank. "));
      jTextAreaIBConfDescription.setWrapStyleWord(true);
      jTextAreaIBConfDescription.setBorder(null);
      jPanelIBConf.add(jTextAreaIBConfDescription, java.awt.BorderLayout.NORTH);

      jPanelIBConfigInner.setBorder(javax.swing.BorderFactory.createLineBorder(new java.awt.Color(0, 0, 0)));
      jPanelIBConfigInner.setAutoscrolls(true);
      jPanelIBConfigInner.setLayout(new java.awt.GridLayout(1, 0));
      jScrollPaneIBConfig.setViewportView(jPanelIBConfigInner);

      jPanelIBConf.add(jScrollPaneIBConfig, java.awt.BorderLayout.CENTER);

      jTabbedPaneTabs.addTab(Main.getLocal().getString("Configure Infiniband"), jPanelIBConf);

      jPanelFrame.add(jTabbedPaneTabs, java.awt.BorderLayout.CENTER);

      jPanelButtons.setLayout(new java.awt.FlowLayout(java.awt.FlowLayout.CENTER, 20, 10));

      jButtonSave.setText(Main.getLocal().getString("Save"));
      jButtonSave.setMaximumSize(new java.awt.Dimension(150, 30));
      jButtonSave.setMinimumSize(new java.awt.Dimension(150, 30));
      jButtonSave.setName("roles"); // NOI18N
      jButtonSave.setPreferredSize(new java.awt.Dimension(150, 30));
      jButtonSave.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonSaveActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonSave);

      jButtonReload.setText(Main.getLocal().getString("Reload from server"));
      jButtonReload.setMaximumSize(new java.awt.Dimension(150, 30));
      jButtonReload.setMinimumSize(new java.awt.Dimension(150, 30));
      jButtonReload.setPreferredSize(new java.awt.Dimension(150, 30));
      jButtonReload.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonReloadActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonReload);
      jPanelButtons.add(fillerButtons);

      jButtonNext.setText(Main.getLocal().getString("Next ->"));
      jButtonNext.setMaximumSize(new java.awt.Dimension(150, 30));
      jButtonNext.setMinimumSize(new java.awt.Dimension(150, 30));
      jButtonNext.setPreferredSize(new java.awt.Dimension(150, 30));
      jButtonNext.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonNextActionPerformed(evt);
         }
      });
      jPanelButtons.add(jButtonNext);

      jPanelFrame.add(jPanelButtons, java.awt.BorderLayout.SOUTH);

      jScrollPane10.setViewportView(jPanelFrame);

      getContentPane().add(jScrollPane10, java.awt.BorderLayout.CENTER);

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
       checkWholeConfigSaved();
       FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

    private void jMenuItemAddMetaActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemAddMetaActionPerformed
       JFrameAddHosts frame = new JFrameAddHosts(this, UpdateDataTypeEnum.HOSTS_META);
       frame.setVisible(true);
}//GEN-LAST:event_jMenuItemAddMetaActionPerformed

    private void jListRoleMetaMouseClicked(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleMetaMouseClicked
    }//GEN-LAST:event_jListRoleMetaMouseClicked

    private void jListRoleMetaMousePressed(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleMetaMousePressed
       if (evt.isPopupTrigger())
       {
          jPopupMenuMeta.show(evt.getComponent(), evt.getX(), evt.getY());
          isSavedRoles = false;
       }
    }//GEN-LAST:event_jListRoleMetaMousePressed

    private void jMenuItemDeleteSelectedMetaActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemDeleteSelectedMetaActionPerformed
       int[] selection = jListRoleMeta.getSelectedIndices();
       for (int i = selection.length - 1; i >= 0; i--)
       {
          ListModel<String> listModel = jListRoleMeta.getModel();
          DefaultListModel<String> model = (DefaultListModel<String>) listModel;
          model.removeElementAt(selection[i]);
       }
}//GEN-LAST:event_jMenuItemDeleteSelectedMetaActionPerformed

    private void jMenuItemImportHostListMetaActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemImportHostListMetaActionPerformed
      JFrameFileChooser frame = new JFrameFileChooser(this, UpdateDataTypeEnum.HOSTS_META);
      frame.setVisible(true);
}//GEN-LAST:event_jMenuItemImportHostListMetaActionPerformed

    private void jListRoleStorageMouseClicked(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleStorageMouseClicked

}//GEN-LAST:event_jListRoleStorageMouseClicked

    private void jListRoleStorageMousePressed(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleStorageMousePressed
       if (evt.isPopupTrigger())
       {
          jPopupMenuStorage.show(evt.getComponent(), evt.getX(), evt.getY());
          isSavedRoles = false;
       }
}//GEN-LAST:event_jListRoleStorageMousePressed

    private void jListRoleClientMouseClicked(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleClientMouseClicked

}//GEN-LAST:event_jListRoleClientMouseClicked

    private void jListRoleClientMousePressed(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleClientMousePressed
       if (evt.isPopupTrigger())
       {
          jPopupMenuClient.show(evt.getComponent(), evt.getX(), evt.getY());
          isSavedRoles = false;
       }
}//GEN-LAST:event_jListRoleClientMousePressed

    private void jMenuItemAddStorageActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemAddStorageActionPerformed
      JFrameAddHosts frame = new JFrameAddHosts(this, UpdateDataTypeEnum.HOSTS_STORAGE);
      frame.setVisible(true);
}//GEN-LAST:event_jMenuItemAddStorageActionPerformed

    private void jMenuItemImportHostListStorageActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemImportHostListStorageActionPerformed
      JFrameFileChooser frame = new JFrameFileChooser(this, UpdateDataTypeEnum.HOSTS_STORAGE);
      frame.setVisible(true);
}//GEN-LAST:event_jMenuItemImportHostListStorageActionPerformed

    private void jMenuItemDeleteSelectedStorageActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemDeleteSelectedStorageActionPerformed
       int[] selection = jListRoleStorage.getSelectedIndices();
       for (int i = selection.length - 1; i >= 0; i--)
       {
          ListModel<String> listModel = jListRoleStorage.getModel();
          DefaultListModel<String> model = (DefaultListModel<String>) listModel;
          model.removeElementAt(selection[i]);
       }
}//GEN-LAST:event_jMenuItemDeleteSelectedStorageActionPerformed

    private void jMenuItemAddClientActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemAddClientActionPerformed
      JFrameAddHosts frame = new JFrameAddHosts(this, UpdateDataTypeEnum.HOSTS_CLIENT);
      frame.setVisible(true);
}//GEN-LAST:event_jMenuItemAddClientActionPerformed

    private void jMenuItemImportHostListClientActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemImportHostListClientActionPerformed
       JFrameFileChooser frame = new JFrameFileChooser(this, UpdateDataTypeEnum.HOSTS_CLIENT);
       frame.setVisible(true);
}//GEN-LAST:event_jMenuItemImportHostListClientActionPerformed

    private void jMenuItemDeleteSelectedClientActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemDeleteSelectedClientActionPerformed
       int[] selection = jListRoleClient.getSelectedIndices();
       for (int i = selection.length - 1; i >= 0; i--)
       {
          ListModel<String> listModel = jListRoleClient.getModel();
          DefaultListModel<String> model = (DefaultListModel<String>) listModel;
          model.removeElementAt(selection[i]);
       }
}//GEN-LAST:event_jMenuItemDeleteSelectedClientActionPerformed

    private void jTabbedPaneTabsStateChanged(javax.swing.event.ChangeEvent evt) {//GEN-FIRST:event_jTabbedPaneTabsStateChanged
       int index = lastTab; // - 1;

       //load the input fields for the nodes
       if (jTabbedPaneTabs.getSelectedIndex() == 2)
       {
          loadIBConfig();
       }

       switch (index)
       {
          case 0:
             if (!isSavedRoles)
             {
                if (confirmNotSaved())
                {
                   isSavedRoles = saveRoles();
                }
             }
             break;
          case 1:
             if (!isSavedConfig)
             {
                if (confirmNotSaved())
                {
                   isSavedConfig = uploadConfig();
                }
             }
             break;
          case 2:
             if (!isSavedIB)
             {
                if (confirmNotSaved())
                {
                   isSavedIB = uploadIBConfig();
                   saveDefaultIBConfig = false;
                }
             }
             else if (saveDefaultIBConfig)
             {
                uploadIBConfig();
                saveDefaultIBConfig = false;
             }
             break;
          default:
             break;
       }

       //update last selected tab
       lastTab = jTabbedPaneTabs.getSelectedIndex();
}//GEN-LAST:event_jTabbedPaneTabsStateChanged

    private void jButtonReloadActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonReloadActionPerformed
       int index = jTabbedPaneTabs.getSelectedIndex();
       switch (index)
       {
          case 0:
             ListModel<String> listModelMeta = jListRoleMeta.getModel();
             DefaultListModel<String> modelMeta = (DefaultListModel<String>) listModelMeta;
             modelMeta.removeAllElements();
             ListModel<String> listModelStorage = jListRoleStorage.getModel();
             DefaultListModel<String> modelStorage = (DefaultListModel<String>) listModelStorage;
             modelStorage.removeAllElements();
             ListModel<String> listModelClient = jListRoleClient.getModel();
             DefaultListModel<String> modelClient = (DefaultListModel<String>) listModelClient;
             modelClient.removeAllElements();
             loadRoles();
             isSavedRoles = true;
             break;
          case 1:
             loadConfig();
             isSavedConfig = true;
             break;
          case 2:
             loadIBConfig();
             isSavedIB = true;
             break;
          default:
             break;
       }
}//GEN-LAST:event_jButtonReloadActionPerformed

    private void jButtonSaveActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonSaveActionPerformed
       int index = jTabbedPaneTabs.getSelectedIndex();
       switch (index)
       {
          case 0:
             isSavedRoles = saveRoles();
             break;
          case 1:
             isSavedConfig = uploadConfig();
             break;
          case 2:
             isSavedIB = uploadIBConfig();
             break;
          default:
             break;
       }
}//GEN-LAST:event_jButtonSaveActionPerformed

   private boolean saveRoles()
   {
      boolean finished = false;

      if (checkSSH())
      {
         metaNodes.clear();
         storageNodes.clear();
         clientNodes.clear();
         boolean uploadSuccessful = uploadRoles();

         if (uploadSuccessful)
         {
            finished = checkDistriArch();
         }
      }

      if (finished)
      {
         JOptionPane.showMessageDialog(null, Main.getLocal().getString("All hosts are successfully saved"), Main.getLocal().getString("Saved roles"),
               JOptionPane.INFORMATION_MESSAGE);
          return true;
      }
      else
      {
         JOptionPane.showMessageDialog(null, Main.getLocal().getString("Couldn't save hosts data!"), Main.getLocal().getString("Saved roles"),
               JOptionPane.ERROR_MESSAGE);
         return false;
      }
   }

   private boolean checkSSH()
   {
      boolean retVal = true;
      InputStream serverResp = null;
      try
      {
         String url = HttpTk.generateAdmonUrl("/XML_CheckSSH");
         int dataSize = jListRoleMeta.getModel().getSize() + jListRoleStorage.getModel().getSize() +
                 jListRoleClient.getModel().getSize();
         int objSize = (2 * dataSize) + 2;
         Object[] obj = new Object[objSize];
         obj[0] = "node";
         obj[1] = jTextRoleFieldMgmtd.getText();
         int i;
         int objCounter = 2;
         for (i = 0; i < jListRoleMeta.getModel().getSize(); i++)
         {
            obj[objCounter] = "node";
            objCounter++;
            String node = jListRoleMeta.getModel().getElementAt(i);
            obj[objCounter] = node;
            objCounter++;
         }
         for (i = 0; i < jListRoleStorage.getModel().getSize(); i++)
         {
            obj[objCounter] = "node";
            objCounter++;
            String node = jListRoleStorage.getModel().getElementAt(i);
            obj[objCounter] = node;
            objCounter++;
         }
         for (i = 0; i < jListRoleClient.getModel().getSize(); i++)
         {
            obj[objCounter] = "node";
            objCounter++;
            String node = jListRoleClient.getModel().getElementAt(i);
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
               Main.getLocal().getString("able to do passwordless SSH login.") + System.lineSeparator() +
               Main.getLocal().getString("This configuration is not saved!") + System.lineSeparator() +
               System.lineSeparator() +Main.getLocal().getString("Failed Hosts : ") + System.lineSeparator() +
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

         return retVal;
      }
      catch (IOException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("IO error"), e);
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error occured"), new Object[]{e, true});
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

    private void jButtonNextActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonNextActionPerformed
       int index = jTabbedPaneTabs.getSelectedIndex();
       switch (index)
       {
          case 0:
             /*
            * This will be handelt by the TabStateChangedListener -> jTabbedPaneTabsStateChanged() if
            * (!isSavedRoles) { if (confirmNotSaved()) { isSavedRoles = saveRoles(); } }
              */
             jTabbedPaneTabs.setSelectedIndex(1);
             break;
          case 1:
             /*
            * This will be handelt by the TabStateChangedListener -> jTabbedPaneTabsStateChanged() if
            * (!isSavedConfig) { if (confirmNotSaved()) { isSavedConfig = uploadConfig(); } }
              */
             //load the input fields for the nodes
             loadIBConfig();
             jTabbedPaneTabs.setSelectedIndex(2);
             break;
          case 2:
             //check if default IB config must be safed
             if (saveDefaultIBConfig)
             {
                uploadIBConfig();
                saveDefaultIBConfig = false;
             } //check if all changes are saved and than open the install dialog
             checkWholeConfigSaved();
             JInternalFrame frame = new JInternalFrameInstall();
             if (!FrameManager.isFrameOpen((JInternalFrameInterface) frame, true))
             {
                Main.getMainWindow().getDesktopPane().add(frame);
                frame.setVisible(true);
                FrameManager.addFrame((JInternalFrameInterface) frame);
             }
             break;
          default:
             break;
       }
    }//GEN-LAST:event_jButtonNextActionPerformed

   public boolean checkWholeConfigSaved()
   {
      //check if all changes are saved and ask the user about save the changes
      if ((!isSavedRoles) || (!isSavedConfig) || (!isSavedIB))
      {
         if (confirmAllNotSaved())
         {
            if (!isSavedRoles)
            {
               isSavedRoles = saveRoles();
            }
            if (!isSavedConfig)
            {
               isSavedConfig = uploadConfig();
            }
            if (!isSavedIB)
            {
               isSavedIB = uploadIBConfig();
            }
         }
      }
      return (isSavedRoles || isSavedConfig || isSavedIB);
   }

    private void formInternalFrameClosing(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosing

    }//GEN-LAST:event_formInternalFrameClosing

    private void formInternalFrameOpened(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameOpened
       isSavedRoles = true;
       isSavedConfig = true;
       isSavedIB = true;
    }//GEN-LAST:event_formInternalFrameOpened

    private void jTextRoleFieldMgmtdActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jTextRoleFieldMgmtdActionPerformed
       isSavedConfig = false;
    }//GEN-LAST:event_jTextRoleFieldMgmtdActionPerformed

    private void jListRoleMetaMouseReleased(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleMetaMouseReleased
       if (evt.isPopupTrigger())
       {
          jPopupMenuMeta.show(evt.getComponent(), evt.getX(), evt.getY());
          isSavedRoles = false;
       }
    }//GEN-LAST:event_jListRoleMetaMouseReleased

    private void jListRoleStorageMouseReleased(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleStorageMouseReleased
       if (evt.isPopupTrigger())
       {
          jPopupMenuStorage.show(evt.getComponent(), evt.getX(), evt.getY());
          isSavedRoles = false;
       }
    }//GEN-LAST:event_jListRoleStorageMouseReleased

    private void jListRoleClientMouseReleased(java.awt.event.MouseEvent evt) {//GEN-FIRST:event_jListRoleClientMouseReleased
       if (evt.isPopupTrigger())
       {
          jPopupMenuClient.show(evt.getComponent(), evt.getX(), evt.getY());
          isSavedRoles = false;
       }
    }//GEN-LAST:event_jListRoleClientMouseReleased

    private void jTextRoleFieldMgmtdKeyTyped(java.awt.event.KeyEvent evt) {//GEN-FIRST:event_jTextRoleFieldMgmtdKeyTyped
       isSavedRoles = false;
    }//GEN-LAST:event_jTextRoleFieldMgmtdKeyTyped

   private void jComboBoxXAttrActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxXAttrActionPerformed
   {//GEN-HEADEREND:event_jComboBoxXAttrActionPerformed

   }//GEN-LAST:event_jComboBoxXAttrActionPerformed

   private void jComboBoxXAttrItemStateChanged(java.awt.event.ItemEvent evt)//GEN-FIRST:event_jComboBoxXAttrItemStateChanged
   {//GEN-HEADEREND:event_jComboBoxXAttrItemStateChanged
      isSavedConfig = false;
   }//GEN-LAST:event_jComboBoxXAttrItemStateChanged

   private void jComboBoxLogLevelActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxLogLevelActionPerformed
   {//GEN-HEADEREND:event_jComboBoxLogLevelActionPerformed

   }//GEN-LAST:event_jComboBoxLogLevelActionPerformed

   private void jComboBoxLogLevelItemStateChanged(java.awt.event.ItemEvent evt)//GEN-FIRST:event_jComboBoxLogLevelItemStateChanged
   {//GEN-HEADEREND:event_jComboBoxLogLevelItemStateChanged
      isSavedConfig = false;
   }//GEN-LAST:event_jComboBoxLogLevelItemStateChanged

   private void jComboBoxClientCacheActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxClientCacheActionPerformed
   {//GEN-HEADEREND:event_jComboBoxClientCacheActionPerformed

   }//GEN-LAST:event_jComboBoxClientCacheActionPerformed

   private void jComboBoxClientCacheItemStateChanged(java.awt.event.ItemEvent evt)//GEN-FIRST:event_jComboBoxClientCacheItemStateChanged
   {//GEN-HEADEREND:event_jComboBoxClientCacheItemStateChanged
      isSavedConfig = false;
   }//GEN-LAST:event_jComboBoxClientCacheItemStateChanged

   private void jCheckBoxUseRDMAActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jCheckBoxUseRDMAActionPerformed
   {//GEN-HEADEREND:event_jCheckBoxUseRDMAActionPerformed

   }//GEN-LAST:event_jCheckBoxUseRDMAActionPerformed

   private void jCheckBoxUseRDMAItemStateChanged(java.awt.event.ItemEvent evt)//GEN-FIRST:event_jCheckBoxUseRDMAItemStateChanged
   {//GEN-HEADEREND:event_jCheckBoxUseRDMAItemStateChanged
      isSavedConfig = false;
   }//GEN-LAST:event_jCheckBoxUseRDMAItemStateChanged

   private void jTextFieldMgmtdDirKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldMgmtdDirKeyTyped
   {//GEN-HEADEREND:event_jTextFieldMgmtdDirKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldMgmtdDirKeyTyped

   private void jTextFieldHelperdPortKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldHelperdPortKeyTyped
   {//GEN-HEADEREND:event_jTextFieldHelperdPortKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldHelperdPortKeyTyped

   private void jTextFieldMgmtdPortKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldMgmtdPortKeyTyped
   {//GEN-HEADEREND:event_jTextFieldMgmtdPortKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldMgmtdPortKeyTyped

   private void jTextFieldClientPortKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldClientPortKeyTyped
   {//GEN-HEADEREND:event_jTextFieldClientPortKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldClientPortKeyTyped

   private void jTextFieldClientMountKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldClientMountKeyTyped
   {//GEN-HEADEREND:event_jTextFieldClientMountKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldClientMountKeyTyped

   private void jTextFieldStorageDirKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldStorageDirKeyTyped
   {//GEN-HEADEREND:event_jTextFieldStorageDirKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldStorageDirKeyTyped

   private void jTextFieldStoragePortKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldStoragePortKeyTyped
   {//GEN-HEADEREND:event_jTextFieldStoragePortKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldStoragePortKeyTyped

   private void jTextFieldMetaPortKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldMetaPortKeyTyped
   {//GEN-HEADEREND:event_jTextFieldMetaPortKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldMetaPortKeyTyped

   private void jTextFieldMetaDirKeyTyped(java.awt.event.KeyEvent evt)//GEN-FIRST:event_jTextFieldMetaDirKeyTyped
   {//GEN-HEADEREND:event_jTextFieldMetaDirKeyTyped
      isSavedConfig = false;
   }//GEN-LAST:event_jTextFieldMetaDirKeyTyped

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.Box.Filler filler1;
   private javax.swing.Box.Filler filler2;
   private javax.swing.Box.Filler fillerButtons;
   private javax.swing.JButton jButtonNext;
   private javax.swing.JButton jButtonReload;
   private javax.swing.JButton jButtonSave;
   private javax.swing.JCheckBox jCheckBoxUseRDMA;
   private javax.swing.JComboBox<String> jComboBoxClientCache;
   private javax.swing.JComboBox<String> jComboBoxLogLevel;
   private javax.swing.JComboBox<String> jComboBoxXAttr;
   private javax.swing.JLabel jLabelClientCache;
   private javax.swing.JLabel jLabelClientMount;
   private javax.swing.JLabel jLabelClientPort;
   private javax.swing.JLabel jLabelHelperdPort;
   private javax.swing.JLabel jLabelInfoClientCache;
   private javax.swing.JLabel jLabelInfoClientMount;
   private javax.swing.JLabel jLabelInfoClientPort;
   private javax.swing.JLabel jLabelInfoHelperdPort;
   private javax.swing.JLabel jLabelInfoLogLevel;
   private javax.swing.JLabel jLabelInfoMetaDir;
   private javax.swing.JLabel jLabelInfoMetaPort;
   private javax.swing.JLabel jLabelInfoMgmtdDir;
   private javax.swing.JLabel jLabelInfoMgmtdPort;
   private javax.swing.JLabel jLabelInfoStorageDir;
   private javax.swing.JLabel jLabelInfoStoragePort;
   private javax.swing.JLabel jLabelInfoUseRDMA;
   private javax.swing.JLabel jLabelInfoXAttr;
   private javax.swing.JLabel jLabelLogLevel;
   private javax.swing.JLabel jLabelMetaDir;
   private javax.swing.JLabel jLabelMetaPort;
   private javax.swing.JLabel jLabelMgmtdDir;
   private javax.swing.JLabel jLabelMgmtdPort;
   private javax.swing.JLabel jLabelRoleMgmtd;
   private javax.swing.JLabel jLabelStorageDir;
   private javax.swing.JLabel jLabelStoragePort;
   private javax.swing.JLabel jLabelUseRDMA;
   private javax.swing.JLabel jLabelXAttr;
   private javax.swing.JList<String> jListRoleClient;
   private javax.swing.JList<String> jListRoleMeta;
   private javax.swing.JList<String> jListRoleStorage;
   private javax.swing.JMenuItem jMenuItemAddClient;
   private javax.swing.JMenuItem jMenuItemAddMeta;
   private javax.swing.JMenuItem jMenuItemAddStorage;
   private javax.swing.JMenuItem jMenuItemDeleteSelectedClient;
   private javax.swing.JMenuItem jMenuItemDeleteSelectedMeta;
   private javax.swing.JMenuItem jMenuItemDeleteSelectedStorage;
   private javax.swing.JMenuItem jMenuItemImportHostListClient;
   private javax.swing.JMenuItem jMenuItemImportHostListMeta;
   private javax.swing.JMenuItem jMenuItemImportHostListStorage;
   private javax.swing.JPanel jPanelBasicConf;
   private javax.swing.JPanel jPanelBasicConfInner;
   private javax.swing.JPanel jPanelButtons;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelIBConf;
   private javax.swing.JPanel jPanelIBConfigInner;
   private javax.swing.JPanel jPanelRoleLists;
   private javax.swing.JPanel jPanelRoleMgmtd;
   private javax.swing.JPanel jPanelRolesConf;
   private javax.swing.JPanel jPanelRolesHeader;
   private javax.swing.JPopupMenu jPopupMenuClient;
   private javax.swing.JPopupMenu jPopupMenuMeta;
   private javax.swing.JPopupMenu jPopupMenuStorage;
   private javax.swing.JScrollPane jScrollPane10;
   private javax.swing.JScrollPane jScrollPaneBasicConf;
   private javax.swing.JScrollPane jScrollPaneIBConfig;
   private javax.swing.JScrollPane jScrollPaneRoleClient;
   private javax.swing.JScrollPane jScrollPaneRoleMeta;
   private javax.swing.JScrollPane jScrollPaneRoleStorage;
   private javax.swing.JTabbedPane jTabbedPaneTabs;
   private javax.swing.JTextArea jTextAreaBasicConfDescription;
   private javax.swing.JTextArea jTextAreaIBConfDescription;
   private javax.swing.JTextArea jTextAreaRolesDescription;
   private javax.swing.JTextField jTextFieldClientMount;
   private javax.swing.JTextField jTextFieldClientPort;
   private javax.swing.JTextField jTextFieldHelperdPort;
   private javax.swing.JTextField jTextFieldMetaDir;
   private javax.swing.JTextField jTextFieldMetaPort;
   private javax.swing.JTextField jTextFieldMgmtdDir;
   private javax.swing.JTextField jTextFieldMgmtdPort;
   private javax.swing.JTextField jTextFieldStorageDir;
   private javax.swing.JTextField jTextFieldStoragePort;
   private javax.swing.JTextArea jTextRoleAreaHintRightClick;
   private javax.swing.JTextField jTextRoleFieldMgmtd;
   // End of variables declaration//GEN-END:variables
}

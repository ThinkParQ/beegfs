package com.beegfs.admon.gui.components.internalframes.management;


import com.beegfs.admon.gui.common.XMLParser;
import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.GuiThread;
import com.beegfs.admon.gui.common.threading.ProgressBarThread;
import com.beegfs.admon.gui.common.tools.CryptTk;
import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.program.Main;
import java.awt.Color;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.Insets;
import java.awt.event.ActionListener;
import java.util.ArrayList;
import java.util.Collections;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JOptionPane;


public class JInternalFrameStartStop extends javax.swing.JInternalFrame implements
   JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameStartStop.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "StartStopGui";

   private transient GetInfoThread infoThread;

   private int componentsPerLine = 0;
   private int lineCount = 0;

   public JInternalFrameStartStop()
   {
      initComponents();

      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());

      updateButtons();
   }

   @Override
   public final String getFrameTitle()
   {
      return Main.getLocal().getString("Start/Stop BeeGFS Services");
   }


   private static class JLabelNode extends JLabel
   {
      private static final long serialVersionUID = 1L;

      private final String node;

      JLabelNode(String nodeID)
      {
         super(nodeID);
         node = nodeID;
      }

      public String getNode()
      {
         return node;
      }
   }


   private static class JLabelStatus extends JLabel
   {
      private static final long serialVersionUID = 1L;

      public static final int STATUS_RUNNING = 0;
      public static final int STATUS_STOPPED = 1;
      private final String node;

      JLabelStatus(String nodeID, int status)
      {
         super();
         if (status == STATUS_RUNNING)
         {
            this.setText(Main.getLocal().getString("Running"));
            this.setForeground(GuiTk.getGreen());
         }
         else
         {
            this.setText(Main.getLocal().getString("Stopped"));
            this.setForeground(GuiTk.getRed());
         }
         node = nodeID;
      }

      public String getNode()
      {
         return node;
      }

      public void setStatus(int status)
      {
         if (status == STATUS_RUNNING)
         {
            this.setText(Main.getLocal().getString("Running"));
            this.setForeground(new Color(0, 140, 50));
         }
         else
         {
            this.setText(Main.getLocal().getString("Stopped"));
            this.setForeground(new Color(180, 0, 30));
         }
         this.revalidate();
      }
   }


   private static class JButtonStart extends JButton
   {
      private static final long serialVersionUID = 1L;

      private final String node;

      JButtonStart(String nodeID)
      {
         super();
         this.setText(Main.getLocal().getString("Start"));
         node = nodeID;
      }

      public String getNode()
      {
         return node;
      }
   }


   private static class JButtonStop extends JButton
   {
      private static final long serialVersionUID = 1L;

      private final String node;

      JButtonStop(String nodeID)
      {
         super();
         this.setText(Main.getLocal().getString("Stop"));
         node = nodeID;
      }

      public String getNode()
      {
         return node;
      }
   }


   private class GetInfoThread extends GuiThread
   {
      private final String service;
      private final ProgressBarThread pBarThread;

      private ArrayList<String> runningHosts;
      private ArrayList<String> stoppedHosts;

      GetInfoThread(String service)
      {
         super(THREAD_NAME);
         this.service = service;

         runningHosts = new ArrayList<>(0);
         stoppedHosts = new ArrayList<>(0);

         pBarThread = new ProgressBarThread(jProgressBar,
            Main.getLocal().getString("Status check in progress... please be patient"), THREAD_NAME + Main.getLocal().getString("ProgressBar"));
      }

      @Override
      public void run()
      {
         pBarThread.start();
         jPanelFrame.revalidate();

         String url = HttpTk.generateAdmonUrl("/XML_StartStop?service=" + service);
         XMLParser parser = new XMLParser(url, THREAD_NAME);
         parser.update();

         try
         {
            runningHosts = parser.getVector("runningHosts");
            Collections.sort(runningHosts);
            stoppedHosts = parser.getVector("stoppedHosts");
            Collections.sort(stoppedHosts);
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication error occured"), new Object[]
            {
               e,
               true
            });
            runningHosts = new ArrayList<>(0);
            stoppedHosts = new ArrayList<>(0);
         }

         this.updateButtonLayout();

         pBarThread.shouldStop();
      }

      private void updateButtonLayout()
      {
         jPanelButtons.removeAll();

         int indexX = 0;
         int indexY = 0;

         for (String nodeID : runningHosts)
         {
            if (stop)
            {
               pBarThread.shouldStop();
               return;
            }

            GridBagConstraints constraints = new GridBagConstraints();
            constraints.fill = GridBagConstraints.NONE;
            constraints.weightx = 0;
            constraints.weighty = 0;
            constraints.gridwidth = 1;
            constraints.gridheight = 1;
            constraints.ipadx = 0;
            constraints.ipady = 0;
            constraints.anchor = GridBagConstraints.CENTER;

            JLabelNode labelID = new JLabelNode(nodeID);
            constraints.gridx = indexX;
            constraints.gridy = indexY;
            constraints.insets = new Insets(5, 15, 5, 5);
            jPanelButtons.add(labelID, constraints);

            final JLabelStatus labelStatus = new JLabelStatus(nodeID, JLabelStatus.STATUS_RUNNING);
            constraints.gridx = indexX + 1;
            constraints.gridy = indexY;
            constraints.insets = new Insets(5, 5, 5, 5);
            jPanelButtons.add(labelStatus, constraints);

            final JButtonStart buttonStart = new JButtonStart(nodeID);
            buttonStart.setEnabled(false);
            constraints.gridx = indexX + 2;
            constraints.gridy = indexY;
            constraints.insets = new Insets(5, 5, 5, 5);
            jPanelButtons.add(buttonStart, constraints);

            final JButtonStop buttonStop = new JButtonStop(nodeID);
            buttonStart.setEnabled(false);
            constraints.gridx = indexX + 3;
            constraints.gridy = indexY;
            constraints.insets = new Insets(5, 5, 5, 15);
            jPanelButtons.add(buttonStop, constraints);

            indexX += 4;
            if(indexX >= componentsPerLine)
            {
               indexX = 0;
               indexY++;
            }

            buttonStart.addActionListener(new ActionListener()
            {
               @Override
               public void actionPerformed(java.awt.event.ActionEvent evt)
               {
                  StartStopHostThread startThread = new StartStopHostThread(
                     StartStopHostThread.ACTION_START, labelStatus, buttonStart,
                     buttonStop, service);
                  startThread.start();
               }
            });

            buttonStop.addActionListener(new ActionListener()
            {
               @Override
               public void actionPerformed(java.awt.event.ActionEvent evt)
               {
                  StartStopHostThread startThread = new StartStopHostThread(
                     StartStopHostThread.ACTION_STOP, labelStatus, buttonStart,
                     buttonStop, service);
                  startThread.start();
               }
            });
         }

         for (String nodeID : stoppedHosts)
         {
            if (stop)
            {
               pBarThread.shouldStop();
               return;
            }

            GridBagConstraints constraints = new GridBagConstraints();
            constraints.fill = GridBagConstraints.NONE;
            constraints.weightx = 0;
            constraints.weighty = 0;
            constraints.gridwidth = 1;
            constraints.gridheight = 1;
            constraints.ipadx = 0;
            constraints.ipady = 0;
            constraints.anchor = GridBagConstraints.CENTER;
            constraints.insets = new Insets(5, 5, 5, 5);

            JLabelNode labelID = new JLabelNode(nodeID);
            constraints.gridx = indexX;
            constraints.gridy = indexY;
            jPanelButtons.add(labelID, constraints);

            final JLabelStatus labelStatus = new JLabelStatus(nodeID, JLabelStatus.STATUS_STOPPED);
            constraints.gridx = indexX + 1;
            constraints.gridy = indexY;
            jPanelButtons.add(labelStatus, constraints);

            final JButtonStart buttonStart = new JButtonStart(nodeID);
            constraints.gridx = indexX + 2;
            constraints.gridy = indexY;
            jPanelButtons.add(buttonStart, constraints);

            final JButtonStop buttonStop = new JButtonStop(nodeID);
            buttonStop.setEnabled(false);
            constraints.gridx = indexX + 3;
            constraints.gridy = indexY;
            jPanelButtons.add(buttonStop, constraints);

            indexX += 4;
            if(indexX >= componentsPerLine)
            {
               indexX = 0;
               indexY++;
            }

            buttonStart.addActionListener(new ActionListener()
            {
               @Override
               public void actionPerformed(java.awt.event.ActionEvent evt)
               {
                  StartStopHostThread startThread = new StartStopHostThread(
                     StartStopHostThread.ACTION_START, labelStatus, buttonStart,
                     buttonStop, service);
                  startThread.start();
               }
            });

            buttonStop.addActionListener(new ActionListener()
            {
               @Override
               public void actionPerformed(java.awt.event.ActionEvent evt)
               {
                  StartStopHostThread startThread = new StartStopHostThread(
                     StartStopHostThread.ACTION_STOP, labelStatus, buttonStart,
                     buttonStop, service);
                  startThread.start();
               }
            });
         }
      }
   }


   private class StartStopHostThread extends GuiThread
   {
      public static final int ACTION_START = 0;
      public static final int ACTION_STOP = 1;
      public static final int ACTION_START_ALL = 2;
      public static final int ACTION_STOP_ALL = 3;
      public static final int ACTION_RESTART_ADMON = 4;

      private final String service;
      private final int action;
      private final String host;
      private final JLabelStatus labelStatus;
      private final JButton buttonStart;
      private final JButton buttonStop;
      private ProgressBarThread pBarThread;
      private final String desc;

      StartStopHostThread(int action, JLabelStatus labelStatus, JButton buttonStart,
         JButton buttonStop, String service)
      {
         super(THREAD_NAME);
         this.service = service;
         stop = false;
         this.action = action;
         this.labelStatus = labelStatus;
         this.buttonStart = buttonStart;
         this.buttonStop = buttonStop;
         this.host = labelStatus.getNode();
         if (service.equals(NodeTypesEnum.METADATA.shortType()))
         {
            desc = Main.getLocal().getString("BeeGFS metadata server");
         }
         else if (service.equals(NodeTypesEnum.STORAGE.shortType()))
         {
            desc = Main.getLocal().getString("BeeGFS storage server");
         }
         else if (service.equals(NodeTypesEnum.MANAGMENT.shortType()))
         {
            desc = Main.getLocal().getString("BeeGFS management daemon");
         }
         else if (service.equals(NodeTypesEnum.CLIENT.shortType()))
         {
            desc = Main.getLocal().getString("BeeGFS client");
         }
         else if (service.equals(NodeTypesEnum.ADMON.shortType()))
         {
            desc = Main.getLocal().getString("Administration and monitoring daemon");
         }
         else
         {
            desc = "";
         }
      }

      @Override
      public void run()
      {
         switch (action)
         {
            case ACTION_START:
               startSingleService();
               break;
            case ACTION_STOP:
               stopSingleService();
               break;
            case ACTION_START_ALL:
               startAllService();
               break;
            case ACTION_STOP_ALL:
               stopAllService();
               break;
            case ACTION_RESTART_ADMON:
               restartAdmon();
               break;
            default:
               break;
         }
      }

      private void restartAdmon()
      {
         pBarThread = new ProgressBarThread(jProgressBar, Main.getLocal().getString("Restarting ") + desc, THREAD_NAME +
            Main.getLocal().getString("ProgressBar"));
         pBarThread.start();

         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();

            String nonceID = parser.getValue("id");
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_StartStop?restartAdmon=true&nonceID=" + nonceID +
               "&secret=" + secret);
            parser.setUrl(url);
            parser.update();

            pBarThread.shouldStop();

            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (!authenticated)
            {
               JOptionPane.showMessageDialog(null,
             Main.getLocal().getString("Operation cannot be performed due to failed authentication"),
             Main.getLocal().getString("Authentication failed"), JOptionPane.ERROR_MESSAGE);
               LOGGER.log(Level.SEVERE,
             Main.getLocal().getString("Authentication failed. You are not allowed to perform this operation!"), true);
               return;
            }
            JOptionPane.showMessageDialog(null,
               Main.getLocal().getString("Restart of Administration and Monitoring daemon finished"), Main.getLocal().getString("Finished"),
               JOptionPane.INFORMATION_MESSAGE);
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication error occured"), new Object[]
            {
               e,
               true
            });
         }
      }

      private void startSingleService()
      {
         pBarThread = new ProgressBarThread(jProgressBar, Main.getLocal().getString("Starting ") + desc + Main.getLocal().getString("on host ") + host,
            THREAD_NAME + Main.getLocal().getString("ProgressBar"));
         pBarThread.start();

         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();

            String nonceID = parser.getValue("id");
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl(
               "/XML_StartStop?start=true&service=" + service + "&host=" + host + "&nonceID=" +
               nonceID + "&secret=" + secret);
            parser.setUrl(url);
            parser.update();

            pBarThread.shouldStop();

            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (!authenticated)
            {
               JOptionPane.showMessageDialog(null,
                  Main.getLocal().getString("Operation cannot be performed due to failed authentication"),
                  Main.getLocal().getString("Authentication failed"), JOptionPane.ERROR_MESSAGE);
               LOGGER.log(Level.SEVERE,
                  Main.getLocal().getString("Authentication failed. You are not allowed to perform this operation!"), true);
               return;
            }

            ArrayList<String> failedHosts = parser.getVector("failedHosts");
            if (failedHosts.isEmpty())
            {
               labelStatus.setStatus(JLabelStatus.STATUS_RUNNING);
               buttonStart.setEnabled(false);
               buttonStop.setEnabled(true);
            }
            else
            {
               // there can only be one element
               String errorStr = Main.getLocal().getString("Failed to start ") + desc + Main.getLocal().getString("on host ") + failedHosts.get(0) + ".";
               JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                  JOptionPane.ERROR_MESSAGE);
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication error occured"), new Object[]
            {
               e,
               true
            });
         }
      }

      private void stopSingleService()
      {
         pBarThread = new ProgressBarThread(jProgressBar, Main.getLocal().getString("Stopping ") + desc + Main.getLocal().getString("on host ") + host,
            THREAD_NAME + Main.getLocal().getString("ProgressBar"));
         pBarThread.start();

         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();

            String nonceID = parser.getValue("id");
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_StartStop?stop=true&service=" + service + "&host=" +
               host + "&nonceID=" + nonceID + "&secret=" + secret);
            parser.setUrl(url);
            parser.update();

            pBarThread.shouldStop();

            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (!authenticated)
            {
               JOptionPane.showMessageDialog(null,
                  Main.getLocal().getString("Operation cannot be performed due to failed authentication"),
                  Main.getLocal().getString("Authentication failed"), JOptionPane.ERROR_MESSAGE);
                  LOGGER.log(Level.SEVERE,
                  Main.getLocal().getString("Authentication failed. You are not allowed to perform this operation!"), true);
               return;
            }

            ArrayList<String> failedHosts = parser.getVector("failedHosts");
            if (failedHosts.isEmpty())
            {
               labelStatus.setStatus(JLabelStatus.STATUS_STOPPED);
               buttonStop.setEnabled(false);
               buttonStart.setEnabled(true);
            }
            else
            {
               // there can only be one element
               String errorStr = Main.getLocal().getString("Failed to stop ") + desc + Main.getLocal().getString("on host ") + failedHosts.get(0) + ".";
               JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                  JOptionPane.ERROR_MESSAGE);
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication error occured"), new Object[]
            {
               e,
               true
            });
         }
      }

      private void startAllService()
      {
         pBarThread = new ProgressBarThread(jProgressBar, Main.getLocal().getString("Starting ") + desc + Main.getLocal().getString("on all hosts"),
            THREAD_NAME + Main.getLocal().getString("ProgressBar"));
         pBarThread.start();

         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();

            String nonceID = parser.getValue("id");
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_StartStop?startAll=true&service=" + service +
               "&nonceID=" + nonceID + "&secret=" + secret);
            parser.setUrl(url);
            parser.update();

            pBarThread.shouldStop();

            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (!authenticated)
            {
               JOptionPane.showMessageDialog(null,
                  Main.getLocal().getString("Operation cannot be performed due to failed authentication"),
                  Main.getLocal().getString("Authentication failed"), JOptionPane.ERROR_MESSAGE);
               LOGGER.log(Level.SEVERE,
                  Main.getLocal().getString("Authentication failed You are not allowed to perform this operation!"), true);
               return;
            }

            ArrayList<String> failedHosts = parser.getVector("failedHosts");
            Component[] comps = jPanelButtons.getComponents();
            for (Component comp : comps)
            {
               if (comp instanceof JLabelStatus)
               {
                  JLabelStatus label = (JLabelStatus) comp;
                  if (!failedHosts.contains(label.getNode()))
                  {
                     label.setStatus(JLabelStatus.STATUS_RUNNING);
                  }
               }

               if (comp instanceof JButtonStart)
               {
                  JButtonStart button = (JButtonStart) comp;
                  if ((!button.getNode().isEmpty()) && (!failedHosts.contains(button.getNode())))
                  {
                     button.setEnabled(false);
                  }
               }

               if (comp instanceof JButtonStop)
               {
                  JButtonStop button = (JButtonStop) comp;
                  if ((!button.getNode().isEmpty()) && (!failedHosts.contains(button.getNode())))
                  {
                     button.setEnabled(true);
                  }
               }
            }

            if (!failedHosts.isEmpty())
            {
               StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
               try
               {
                  for (String hostString : failedHosts)
                  {
                     errorStr.append(System.lineSeparator());
                     errorStr.append(Main.getLocal().getString("Failed to start "));
                     errorStr.append(desc);
                     errorStr.append(Main.getLocal().getString("on host "));
                     errorStr.append(hostString);
                  }
               }
               catch (NullPointerException e)
               {
            	   errorStr = new StringBuilder(Main.getLocal().getString("Failed to stop hosts."));
               }
               JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                  JOptionPane.ERROR_MESSAGE);
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication error occured"), new Object[]
            {
               e,
               true
            });
         }
      }

      private void stopAllService()
      {
         pBarThread = new ProgressBarThread(jProgressBar, "Stopping " + desc + " on all hosts",
            THREAD_NAME + "ProgressBar");
         pBarThread.start();

         try
         {
            String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
            XMLParser parser = new XMLParser(url, THREAD_NAME);
            parser.update();

            String nonceID = parser.getValue("id");
            long nonce = Long.parseLong(parser.getValue("nonce"));
            String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);
            url = HttpTk.generateAdmonUrl("/XML_StartStop?stopAll=true&service=" + service +
               "&nonceID=" + nonceID + "&secret=" + secret);
            parser.setUrl(url);
            parser.update();

            pBarThread.shouldStop();

            boolean authenticated = Boolean.parseBoolean(parser.getValue("authenticated"));
            if (!authenticated)
            {
               JOptionPane.showMessageDialog(null,
            	Main.getLocal().getString("Operation cannot be performed due to failed authentication"),
            	Main.getLocal().getString("Authentication failed"), JOptionPane.ERROR_MESSAGE);
               LOGGER.log(Level.SEVERE,
                  Main.getLocal().getString("Authentication failed. You are not allowed to perform this operation!"), true);
               return;
            }

            ArrayList<String> failedHosts = parser.getVector("failedHosts");
            Component[] comps = jPanelButtons.getComponents();
            for (Component comp : comps)
            {
               if (comp instanceof JLabelStatus)
               {
                  JLabelStatus label = (JLabelStatus) comp;
                  if (!failedHosts.contains(label.getNode()))
                  {
                     label.setStatus(JLabelStatus.STATUS_STOPPED);
                  }
               }

               if (comp instanceof JButtonStart)
               {
                  JButtonStart button = (JButtonStart) comp;
                  if ((!button.getNode().isEmpty()) && (!failedHosts.contains(button.getNode())))
                  {
                     button.setEnabled(true);
                  }
               }

               if (comp instanceof JButtonStop)
               {
                  JButtonStop button = (JButtonStop) comp;
                  if ((!button.getNode().isEmpty()) && (!failedHosts.contains(button.getNode())))
                  {
                     button.setEnabled(false);
                  }
               }
            }

            if (!failedHosts.isEmpty())
            {
               StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
               try
               {
                  for (String hostString : failedHosts)
                  {
                     errorStr.append(System.lineSeparator());
                     errorStr.append(Main.getLocal().getString("Failed to stop "));
                     errorStr.append(desc);
                     errorStr.append(Main.getLocal().getString("on host "));
                     errorStr.append(hostString);
                  }
               }
               catch (java.lang.NullPointerException e)
               {
            	   errorStr = new StringBuilder(Main.getLocal().getString("Failed to stop nodes."));
               }
               JOptionPane.showMessageDialog(null, errorStr, Main.getLocal().getString("Errors occured"),
                  JOptionPane.ERROR_MESSAGE);
            }
         }
         catch (CommunicationException e)
         {
            LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication error occured"), new Object[]
            {
               e,
               true
            });
         }
      }
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameStartStop;
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
      jProgressBar = new javax.swing.JProgressBar();
      jPanelStartStop = new javax.swing.JPanel();
      jPanelControll = new javax.swing.JPanel();
      jComboBoxNodeType =  new javax.swing.JComboBox<>();
      jButtonStartAll = new javax.swing.JButton();
      jButtonStopAll = new javax.swing.JButton();
      jButtonRestartAdmon = new javax.swing.JButton();
      jPanelButtons = new javax.swing.JPanel();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setPreferredSize(new java.awt.Dimension(680, 350));
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
      addComponentListener(new java.awt.event.ComponentAdapter()
      {
         public void componentResized(java.awt.event.ComponentEvent evt)
         {
            formComponentResized(evt);
         }
      });

      jScrollPaneFrame.setPreferredSize(new java.awt.Dimension(660, 350));

      jPanelFrame.setBorder(javax.swing.BorderFactory.createEmptyBorder(10, 10, 10, 10));
      jPanelFrame.setMinimumSize(new java.awt.Dimension(0, 0));
      jPanelFrame.setPreferredSize(new java.awt.Dimension(660, 300));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jProgressBar.setString("");
      jPanelFrame.add(jProgressBar, java.awt.BorderLayout.NORTH);

      jPanelStartStop.setMinimumSize(new java.awt.Dimension(620, 80));
      jPanelStartStop.setPreferredSize(new java.awt.Dimension(620, 260));
      jPanelStartStop.setLayout(new java.awt.BorderLayout(5, 5));

      jPanelControll.setBorder(javax.swing.BorderFactory.createEtchedBorder());
      jPanelControll.setMinimumSize(new java.awt.Dimension(600, 40));
      jPanelControll.setPreferredSize(new java.awt.Dimension(620, 40));
      jPanelControll.setLayout(new java.awt.GridBagLayout());

      jComboBoxNodeType.setModel(new javax.swing.DefaultComboBoxModel<>(new String[] { Main.getLocal().getString("1 - Management Daemon"), Main.getLocal().getString("2 - Metadata Daemon"), Main.getLocal().getString("3 - Storage Daemon"), Main.getLocal().getString("4 - Client") }));
      jComboBoxNodeType.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jComboBoxNodeTypeActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 15);
      jPanelControll.add(jComboBoxNodeType, gridBagConstraints);

      jButtonStartAll.setText(Main.getLocal().getString("Start all"));
      jButtonStartAll.setMaximumSize(new java.awt.Dimension(80, 30));
      jButtonStartAll.setMinimumSize(new java.awt.Dimension(80, 30));
      jButtonStartAll.setPreferredSize(new java.awt.Dimension(80, 30));
      jButtonStartAll.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonStartAllActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 15, 5, 5);
      jPanelControll.add(jButtonStartAll, gridBagConstraints);

      jButtonStopAll.setText(Main.getLocal().getString("Stop all"));
      jButtonStopAll.setMaximumSize(new java.awt.Dimension(80, 30));
      jButtonStopAll.setMinimumSize(new java.awt.Dimension(80, 30));
      jButtonStopAll.setPreferredSize(new java.awt.Dimension(80, 30));
      jButtonStopAll.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonStopAllActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 2;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 15);
      jPanelControll.add(jButtonStopAll, gridBagConstraints);

      jButtonRestartAdmon.setText(Main.getLocal().getString("Restart Admon"));
      jButtonRestartAdmon.setMaximumSize(new java.awt.Dimension(130, 30));
      jButtonRestartAdmon.setMinimumSize(new java.awt.Dimension(130, 30));
      jButtonRestartAdmon.setPreferredSize(new java.awt.Dimension(130, 30));
      jButtonRestartAdmon.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonRestartAdmonActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 3;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.insets = new java.awt.Insets(5, 15, 5, 5);
      jPanelControll.add(jButtonRestartAdmon, gridBagConstraints);

      jPanelStartStop.add(jPanelControll, java.awt.BorderLayout.NORTH);

      jPanelButtons.setPreferredSize(new java.awt.Dimension(620, 200));
      jPanelButtons.setLayout(new java.awt.GridBagLayout());
      jPanelStartStop.add(jPanelButtons, java.awt.BorderLayout.CENTER);

      jPanelFrame.add(jPanelStartStop, java.awt.BorderLayout.CENTER);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      getContentPane().add(jScrollPaneFrame, java.awt.BorderLayout.CENTER);

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

   private void jButtonStartAllActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonStartAllActionPerformed
   {//GEN-HEADEREND:event_jButtonStartAllActionPerformed
      int index = jComboBoxNodeType.getSelectedIndex();
      String service = selectedIndexToType(index);

      StartStopHostThread startThread = new StartStopHostThread(
         StartStopHostThread.ACTION_START_ALL, new JLabelStatus("", -1), jButtonStartAll,
         jButtonStopAll, service);
      startThread.start();
   }//GEN-LAST:event_jButtonStartAllActionPerformed

   private void jButtonStopAllActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonStopAllActionPerformed
   {//GEN-HEADEREND:event_jButtonStopAllActionPerformed
      int index = jComboBoxNodeType.getSelectedIndex();
      String service = selectedIndexToType(index);

      StartStopHostThread startThread = new StartStopHostThread(
         StartStopHostThread.ACTION_STOP_ALL, new JLabelStatus("", -1), jButtonStartAll,
         jButtonStopAll, service);
      startThread.start();
   }//GEN-LAST:event_jButtonStopAllActionPerformed

   private void jComboBoxNodeTypeActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxNodeTypeActionPerformed
   {//GEN-HEADEREND:event_jComboBoxNodeTypeActionPerformed
      updateButtons();
   }//GEN-LAST:event_jComboBoxNodeTypeActionPerformed

   private void jButtonRestartAdmonActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jButtonRestartAdmonActionPerformed
   {//GEN-HEADEREND:event_jButtonRestartAdmonActionPerformed
      StartStopHostThread startThread = new StartStopHostThread(
         StartStopHostThread.ACTION_RESTART_ADMON, new JLabelStatus("", -1), jButtonRestartAdmon,
         jButtonRestartAdmon, "admon");
      startThread.start();
   }//GEN-LAST:event_jButtonRestartAdmonActionPerformed

   private void formComponentResized(java.awt.event.ComponentEvent evt)//GEN-FIRST:event_formComponentResized
   {//GEN-HEADEREND:event_formComponentResized
      calculateButtonLayout();
      infoThread.updateButtonLayout();
   }//GEN-LAST:event_formComponentResized

   private void updateButtons()
   {
      int index = jComboBoxNodeType.getSelectedIndex();
      String service = selectedIndexToType(index);

      infoThread = new GetInfoThread(service);
      infoThread.start();
   }

   private String selectedIndexToType(int index)
   {
      String retVal;

      switch(index)
      {
         case 0:
            retVal = NodeTypesEnum.MANAGMENT.shortType();
            break;
         case 1:
            retVal = NodeTypesEnum.METADATA.shortType();
            break;
         case 2:
            retVal = NodeTypesEnum.STORAGE.shortType();
            break;
         case 3:
            retVal = NodeTypesEnum.CLIENT.shortType();
            break;
         default:
            retVal = "";
            break;
      }
      return retVal;
   }

   private void calculateButtonLayout()
   {
      int maxWidthHostLabel = 0;
      int maxWidthStatusLabel = 0;
      int maxWidthStartButton = 0;
      int maxWidthStopButton = 0;

      Component[] components = jPanelButtons.getComponents();
      int componentCount = components.length;

      // fist init of the GUI
      if(componentCount == 0)
      {
         int defaultComponentsPerLine = 8;
         this.componentsPerLine = defaultComponentsPerLine;

         int numLabels = this.jComboBoxNodeType.getItemCount();
         this.lineCount = numLabels / this.componentsPerLine;
         if( (this.lineCount == 0 ) || (componentCount % this.componentsPerLine) != 0)
         {
            this.lineCount++;
         }

         return;
      }

      for (Component component : components)
      {
         Dimension componentSize = component.getPreferredSize();
         if(component instanceof JLabelNode)
         {
            if(maxWidthHostLabel < componentSize.width)
            {
               maxWidthHostLabel = componentSize.width;
            }
         }
         else
         if(component instanceof JLabelStatus)
         {
            if(maxWidthStatusLabel < componentSize.width)
            {
               maxWidthStatusLabel = componentSize.width;
            }
         }
         else
         if(component instanceof JButtonStart)
         {
            if(maxWidthStartButton < componentSize.width)
            {
               maxWidthStartButton = componentSize.width;
            }
         }
         else
         if(component instanceof JButtonStop)
         {
            if(maxWidthStopButton < componentSize.width)
            {
               maxWidthStopButton = componentSize.width;
            }
         }
      }

      int singleWidth = maxWidthHostLabel + maxWidthStatusLabel + maxWidthStartButton +
         maxWidthStopButton;
      if(singleWidth <= 0)
      {
         return;
      }

      Dimension scrollPanelSize = jPanelFrame.getSize();
      int scrollPanelWidth = scrollPanelSize.width - 20; // subtract outer borders

      // devide by size of all components + insets width and multiply by four (2 label + 2 buttons)
      this.componentsPerLine = (scrollPanelWidth / (singleWidth + 60)) * 4;
      if(this.componentsPerLine == 0)
      {
         this.componentsPerLine = 4;
      }

      this.lineCount = componentCount / this.componentsPerLine;
      if( (this.lineCount == 0 ) || (componentCount % this.componentsPerLine) != 0)
      {
         this.lineCount++;
      }
   }

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JButton jButtonRestartAdmon;
   private javax.swing.JButton jButtonStartAll;
   private javax.swing.JButton jButtonStopAll;
   private javax.swing.JComboBox<String> jComboBoxNodeType;
   private javax.swing.JPanel jPanelButtons;
   private javax.swing.JPanel jPanelControll;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelStartStop;
   private javax.swing.JProgressBar jProgressBar;
   private javax.swing.JScrollPane jScrollPaneFrame;
   // End of variables declaration//GEN-END:variables

}

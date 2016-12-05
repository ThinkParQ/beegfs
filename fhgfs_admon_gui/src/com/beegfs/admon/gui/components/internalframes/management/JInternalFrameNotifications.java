package com.beegfs.admon.gui.components.internalframes.management;

import com.beegfs.admon.gui.common.XMLParser;
import static com.beegfs.admon.gui.common.enums.SmtpSendTypeEnum.SMTP_SEND_TYPE_SENDMAIL;
import static com.beegfs.admon.gui.common.enums.SmtpSendTypeEnum.SMTP_SEND_TYPE_SOCKET;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.tools.CryptTk;
import static com.beegfs.admon.gui.common.tools.DefinesTk.DEFAULT_STRING_LENGTH;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.program.Main;
import com.myjavatools.web.ClientHttpRequest;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.TreeMap;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JOptionPane;

public class JInternalFrameNotifications extends javax.swing.JInternalFrame
   implements JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameNotifications.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "Notification";

   private String lastSmtpServer = "";
   private String lastSendmailPath = "";

   /**
    * Creates new form JInternalFrameMetaNode
    */
   public JInternalFrameNotifications()
   {
      initComponents();
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
      loadMailSettings();
      loadScriptSettings();
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameNotifications;
   }

   private void setMailSettings(boolean enable, boolean ignoreEnableCheckbox)
   {
      if(!ignoreEnableCheckbox)
      {
         jCheckBoxEnable.setEnabled(enable);
      }

      jButtonSubmitMail.setEnabled(enable);
      jComboBoxSendType.setEnabled(enable);
      jTextFieldSmtp.setEnabled(enable);
      jTextFieldSender.setEnabled(enable);
      jTextFieldRecipient.setEnabled(enable);
      jTextFieldDelay.setEnabled(enable);
      jTextFieldResend.setEnabled(enable);
   }

   private void loadMailSettings()
   {
      String url = HttpTk.generateAdmonUrl("/XML_MailNotification");
      XMLParser parser = new XMLParser(url, THREAD_NAME);
      parser.update();
      
      try
      {
         TreeMap<String, String> data = parser.getTreeMap();
         if (Boolean.parseBoolean(data.get("enabled")))
         {
            jCheckBoxEnable.setSelected(true);
         }
         else
         {
            jCheckBoxEnable.setSelected(false);
         }
         jComboBoxSendType.setSelectedIndex(Integer.parseInt(data.get("sendType")));

         lastSendmailPath = data.get("sendmailPath");
         lastSmtpServer = data.get("smtpServer");
         if(jComboBoxSendType.getSelectedIndex() == SMTP_SEND_TYPE_SOCKET.ordinal())
         {
            jTextFieldSmtp.setText(lastSmtpServer);
         }
         else if(jComboBoxSendType.getSelectedIndex() == SMTP_SEND_TYPE_SENDMAIL.ordinal())
         {
            jTextFieldSmtp.setText(lastSendmailPath);
         }

         jTextFieldSender.setText(data.get("sender"));
         jTextFieldRecipient.setText(data.get("recipient"));
         jTextFieldDelay.setText(data.get("delay"));
         jTextFieldResend.setText(data.get("resendTime"));

         setMailSettings(jCheckBoxEnable.isSelected(), true);
         setLabelAfterComboboxChanged();

         boolean overrideActive = Boolean.parseBoolean(data.get("overrideActive"));
         setMailSettings(!overrideActive, false);
         jLabelDisableInfo.setVisible(overrideActive);
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
         {
            e,
            true
         });
      }
   }

   private void uploadMailConfig()
   {
      InputStream serverResp = null;
      try
      {
         boolean mailEnabled = false;
         if (jCheckBoxEnable.isSelected())
         {
            mailEnabled = true;
         }

         String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
         // get a nonce for authentication from server
         XMLParser parser = new XMLParser(url, THREAD_NAME);
         parser.update();
         TreeMap<String, String> data = parser.getTreeMap();
         int nonceID = Integer.parseInt(data.get("id"));
         long nonce = Long.parseLong(data.get("nonce"));
         String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);

         if(jComboBoxSendType.getSelectedIndex() == SMTP_SEND_TYPE_SOCKET.ordinal())
         {
            lastSmtpServer = jTextFieldSmtp.getText();
         }
         else if(jComboBoxSendType.getSelectedIndex() == SMTP_SEND_TYPE_SENDMAIL.ordinal())
         {
            lastSendmailPath = jTextFieldSmtp.getText();
         }

         Object[] obj = new Object[]
         {
            "change", "true",
            "mailEnabled", String.valueOf(mailEnabled),
            "sendType", String.valueOf(jComboBoxSendType.getSelectedIndex()),
            "sendmailPath", lastSendmailPath,
            "smtpServer", lastSmtpServer,
            "sender", jTextFieldSender.getText(),
            "recipient", jTextFieldRecipient.getText(),
            "mailMinDownTimeSec", jTextFieldDelay.getText(),
            "mailResendMailTimeMin", jTextFieldResend.getText(),
            "nonceID", nonceID,
            "secret", secret
         };

         url = HttpTk.generateAdmonUrl("/XML_MailNotification");
         serverResp = ClientHttpRequest.post(new java.net.URL(url), obj);
         parser.readFromInputStream(serverResp);
         ArrayList<String> errors = parser.getVector("errors");
         if (errors.isEmpty())
         {
            JOptionPane.showMessageDialog(null,
                    "Configuration successfully written", "Success",
                    JOptionPane.INFORMATION_MESSAGE);
         }
         else
         {
            StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
            for (String error : errors)
            {
               errorStr.append(error);
               errorStr.append(System.lineSeparator());
            }

            JOptionPane.showMessageDialog(null, errorStr, "Errors occured",
                    JOptionPane.ERROR_MESSAGE);
         }
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
   }

   private void loadScriptSettings()
   {
      String url = HttpTk.generateAdmonUrl("/XML_ScriptSettings");
      XMLParser parser = new XMLParser(url, THREAD_NAME);
      parser.update();
      
      try
      {
         TreeMap<String, String> data = parser.getTreeMap();
         jTextFieldScript.setText(data.get("scriptPath"));
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
         {
            e,
            true
         });
      }
   }

   private void sendTestEmail()
   {
      String url = HttpTk.generateAdmonUrl("/XML_TestEMail");
      XMLParser parser = new XMLParser(url, THREAD_NAME);
      parser.update();
      try
      {
         TreeMap<String, String> data = parser.getTreeMap();
         if (!Boolean.parseBoolean(data.get("success")))
         {
            JOptionPane.showMessageDialog(Main.getMainWindow(),
                    "Couldn't communicate with SMTP server.", "eMail send failure",
                    JOptionPane.ERROR_MESSAGE);
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, "Communication Error occured", new Object[]
         {
            e,
            true
         });
      }
   }

   private void uploadScriptSettings()
   {
      InputStream serverResp = null;
      try
      {
         String url = HttpTk.generateAdmonUrl("/XML_GetNonce");
         // get a nonce for authentication from server
         XMLParser parser = new XMLParser(url, THREAD_NAME);
         parser.update();
         TreeMap<String, String> data = parser.getTreeMap();
         int nonceID = Integer.parseInt(data.get("id"));
         long nonce = Long.parseLong(data.get("nonce"));
         String secret = CryptTk.cryptWithNonce(Main.getSession().getPw(), nonce);

         Object[] obj = new Object[]
         {
            "change", "true",
            "scriptPath", jTextFieldScript.getText().trim(),
            "nonceID", nonceID,
            "secret", secret
         };

         url = HttpTk.generateAdmonUrl("/XML_ScriptSettings");
         serverResp = ClientHttpRequest.post(new java.net.URL(url), obj);
         parser.readFromInputStream(serverResp);
         ArrayList<String> errors = parser.getVector("errors");
         if (errors.isEmpty())
         {
            JOptionPane.showMessageDialog(null, "Configuration successfully written", "Success",
                    JOptionPane.INFORMATION_MESSAGE);
         }
         else
         {
            StringBuilder errorStr = new StringBuilder(DEFAULT_STRING_LENGTH);
            for (String error : errors)
            {
               errorStr.append(error);
               errorStr.append(System.lineSeparator());
            }
            JOptionPane.showMessageDialog(null, errorStr, "Errors occured",
                    JOptionPane.ERROR_MESSAGE);
         }
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
   }

   private void setLabelAfterComboboxChanged()
   {
      if(jComboBoxSendType.getSelectedIndex() == SMTP_SEND_TYPE_SOCKET.ordinal())
      {
         jLabelSmtp.setText("Smtp server to use for sending mails:");
         jTextFieldSmtp.setText(lastSmtpServer);
      }
      else if(jComboBoxSendType.getSelectedIndex() == SMTP_SEND_TYPE_SENDMAIL.ordinal())
      {
         jLabelSmtp.setText("Path to sendmail binary:");
         jTextFieldSmtp.setText(lastSendmailPath);
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
      jTabbedPaneSettings = new javax.swing.JTabbedPane();
      jPanelMail = new javax.swing.JPanel();
      jCheckBoxEnable = new javax.swing.JCheckBox();
      jLabelSendType = new javax.swing.JLabel();
      jComboBoxSendType = new javax.swing.JComboBox<>();
      jLabelSmtp = new javax.swing.JLabel();
      jTextFieldSmtp = new javax.swing.JTextField();
      jTextFieldSender = new javax.swing.JTextField();
      jLabelSender = new javax.swing.JLabel();
      jTextFieldRecipient = new javax.swing.JTextField();
      jLabelRecipient = new javax.swing.JLabel();
      jTextFieldDelay = new javax.swing.JTextField();
      jLabelDelay = new javax.swing.JLabel();
      jTextFieldResend = new javax.swing.JTextField();
      jLabelResend = new javax.swing.JLabel();
      jButtonSubmitMail = new javax.swing.JButton();
      jButtonSendEMail = new javax.swing.JButton();
      jLabelDisableInfo = new javax.swing.JLabel();
      jLabelEnable = new javax.swing.JLabel();
      jPanelScripts = new javax.swing.JPanel();
      jButtonSubmitScript = new javax.swing.JButton();
      jTextArea3 = new javax.swing.JTextArea();
      jLabel6 = new javax.swing.JLabel();
      jTextFieldScript = new javax.swing.JTextField();

      setClosable(true);
      setIconifiable(true);
      setMaximizable(true);
      setResizable(true);
      setPreferredSize(new java.awt.Dimension(859, 400));
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
      jPanelFrame.setPreferredSize(new java.awt.Dimension(830, 300));
      jPanelFrame.setLayout(new java.awt.BorderLayout());

      jTabbedPaneSettings.setPreferredSize(new java.awt.Dimension(839, 292));

      jPanelMail.setBorder(javax.swing.BorderFactory.createTitledBorder("Mail Notifications"));
      jPanelMail.setMinimumSize(new java.awt.Dimension(732, 310));
      jPanelMail.setPreferredSize(new java.awt.Dimension(732, 310));
      jPanelMail.setLayout(new java.awt.GridBagLayout());

      jCheckBoxEnable.setHorizontalTextPosition(javax.swing.SwingConstants.LEADING);
      jCheckBoxEnable.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jCheckBoxEnableActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jCheckBoxEnable, gridBagConstraints);

      jLabelSendType.setText("Methode to send the mails:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 2;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelSendType, gridBagConstraints);

      jComboBoxSendType.setMaximumRowCount(2);
      jComboBoxSendType.setModel(new javax.swing.DefaultComboBoxModel<>(new String[] { "socket", "sendmail"}));
      jComboBoxSendType.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jComboBoxSendTypeActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 2;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jComboBoxSendType, gridBagConstraints);

      jLabelSmtp.setText("Smtp server to use for sending mails:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 3;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelSmtp, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 3;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jTextFieldSmtp, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 4;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jTextFieldSender, gridBagConstraints);

      jLabelSender.setText("Sender eMail address to use:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 4;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelSender, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 5;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jTextFieldRecipient, gridBagConstraints);

      jLabelRecipient.setText("Recipient address, to which notifications are sent:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 5;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelRecipient, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 6;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jTextFieldDelay, gridBagConstraints);

      jLabelDelay.setText("Minimum amount of time between a event and the mail notification in seconds:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 6;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelDelay, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 7;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jTextFieldResend, gridBagConstraints);

      jLabelResend.setText("Amount of time after which an already sent eMail is resent as reminder in minutes:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 7;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelResend, gridBagConstraints);

      jButtonSubmitMail.setText("Submit");
      jButtonSubmitMail.setMaximumSize(new java.awt.Dimension(100, 30));
      jButtonSubmitMail.setMinimumSize(new java.awt.Dimension(100, 30));
      jButtonSubmitMail.setPreferredSize(new java.awt.Dimension(100, 30));
      jButtonSubmitMail.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonSubmitMailActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 8;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.insets = new java.awt.Insets(10, 10, 10, 10);
      jPanelMail.add(jButtonSubmitMail, gridBagConstraints);

      jButtonSendEMail.setText("Send eMail");
      jButtonSendEMail.setMaximumSize(new java.awt.Dimension(100, 30));
      jButtonSendEMail.setMinimumSize(new java.awt.Dimension(100, 30));
      jButtonSendEMail.setPreferredSize(new java.awt.Dimension(100, 30));
      jButtonSendEMail.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonSendEMailActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 8;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(10, 10, 10, 10);
      jPanelMail.add(jButtonSendEMail, gridBagConstraints);

      jLabelDisableInfo.setForeground(java.awt.Color.red);
      jLabelDisableInfo.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
      jLabelDisableInfo.setText("This dialog is disabled by the admon configuration file (override).");
      jLabelDisableInfo.setRequestFocusEnabled(false);
      jLabelDisableInfo.setVerifyInputWhenFocusTarget(false);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.gridwidth = 2;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.NORTH;
      jPanelMail.add(jLabelDisableInfo, gridBagConstraints);

      jLabelEnable.setText("Enable eMail notifications:");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelMail.add(jLabelEnable, gridBagConstraints);

      jTabbedPaneSettings.addTab("Mail Notifications", jPanelMail);

      jPanelScripts.setBorder(javax.swing.BorderFactory.createTitledBorder("Run Script"));
      jPanelScripts.setLayout(new java.awt.GridBagLayout());

      jButtonSubmitScript.setText("Submit");
      jButtonSubmitScript.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jButtonSubmitScriptActionPerformed(evt);
         }
      });
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 2;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(10, 10, 10, 10);
      jPanelScripts.add(jButtonSubmitScript, gridBagConstraints);

      jTextArea3.setEditable(false);
      jTextArea3.setBackground(new java.awt.Color(238, 238, 238));
      jTextArea3.setColumns(20);
      jTextArea3.setLineWrap(true);
      jTextArea3.setRows(5);
      jTextArea3.setText("Please define a script to run whenever a node is reported as down. The script must reside on the server, where the beegfs-admon process is running. Please make sure your script contains a valid header (e.g. \"#!/bin/bash\").\n\nThe following variables are automatically passed to the script :\n\n$1 : The service, which is reported (can be meta or storage)\n$2 : The hostname of the reported node\n\nTo stop script execution please submit an empty line.");
      jTextArea3.setWrapStyleWord(true);
      jTextArea3.setBorder(null);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 0;
      gridBagConstraints.gridwidth = 2;
      gridBagConstraints.fill = java.awt.GridBagConstraints.BOTH;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.SOUTH;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.weighty = 1.0;
      jPanelScripts.add(jTextArea3, gridBagConstraints);

      jLabel6.setText("Path to script : ");
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 0;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.EAST;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelScripts.add(jLabel6, gridBagConstraints);
      gridBagConstraints = new java.awt.GridBagConstraints();
      gridBagConstraints.gridx = 1;
      gridBagConstraints.gridy = 1;
      gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
      gridBagConstraints.anchor = java.awt.GridBagConstraints.WEST;
      gridBagConstraints.weightx = 1.0;
      gridBagConstraints.insets = new java.awt.Insets(5, 5, 5, 5);
      jPanelScripts.add(jTextFieldScript, gridBagConstraints);

      jTabbedPaneSettings.addTab("Run Script", jPanelScripts);

      jPanelFrame.add(jTabbedPaneSettings, java.awt.BorderLayout.CENTER);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 848, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 344, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
      FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed

    private void jCheckBoxEnableActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jCheckBoxEnableActionPerformed
      setMailSettings(jCheckBoxEnable.isSelected(), true);
    }//GEN-LAST:event_jCheckBoxEnableActionPerformed

    private void jButtonSubmitMailActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonSubmitMailActionPerformed
      uploadMailConfig();
}//GEN-LAST:event_jButtonSubmitMailActionPerformed

    private void jButtonSubmitScriptActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonSubmitScriptActionPerformed
      uploadScriptSettings();
}//GEN-LAST:event_jButtonSubmitScriptActionPerformed

    private void jButtonSendEMailActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jButtonSendEMailActionPerformed
      sendTestEmail();
    }//GEN-LAST:event_jButtonSendEMailActionPerformed

   private void jComboBoxSendTypeActionPerformed(java.awt.event.ActionEvent evt)//GEN-FIRST:event_jComboBoxSendTypeActionPerformed
   {//GEN-HEADEREND:event_jComboBoxSendTypeActionPerformed
      setLabelAfterComboboxChanged();
   }//GEN-LAST:event_jComboBoxSendTypeActionPerformed

   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JButton jButtonSendEMail;
   private javax.swing.JButton jButtonSubmitMail;
   private javax.swing.JButton jButtonSubmitScript;
   private javax.swing.JCheckBox jCheckBoxEnable;
   private javax.swing.JComboBox<String> jComboBoxSendType;
   private javax.swing.JLabel jLabel6;
   private javax.swing.JLabel jLabelDelay;
   private javax.swing.JLabel jLabelDisableInfo;
   private javax.swing.JLabel jLabelEnable;
   private javax.swing.JLabel jLabelRecipient;
   private javax.swing.JLabel jLabelResend;
   private javax.swing.JLabel jLabelSendType;
   private javax.swing.JLabel jLabelSender;
   private javax.swing.JLabel jLabelSmtp;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JPanel jPanelMail;
   private javax.swing.JPanel jPanelScripts;
   private javax.swing.JScrollPane jScrollPaneFrame;
   private javax.swing.JTabbedPane jTabbedPaneSettings;
   private javax.swing.JTextArea jTextArea3;
   private javax.swing.JTextField jTextFieldDelay;
   private javax.swing.JTextField jTextFieldRecipient;
   private javax.swing.JTextField jTextFieldResend;
   private javax.swing.JTextField jTextFieldScript;
   private javax.swing.JTextField jTextFieldSender;
   private javax.swing.JTextField jTextFieldSmtp;
   // End of variables declaration//GEN-END:variables

   @Override
   public final String getFrameTitle()
   {
      return "Notification Settings";
   }
}

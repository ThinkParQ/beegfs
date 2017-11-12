package com.beegfs.admon.gui.components;


import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.exceptions.WrongBackendVersionException;
import com.beegfs.admon.gui.common.nodes.NodeEnvironment;
import com.beegfs.admon.gui.common.tools.CryptTk;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.components.dialogs.JDialogAbout;
import com.beegfs.admon.gui.components.dialogs.JDialogLogin;
import com.beegfs.admon.gui.components.dialogs.JDialogNewConfig;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameKnownProblems;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameNotifications;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameUserSettings;
import com.beegfs.admon.gui.components.lf.BeegfsLookAndFeel;
import com.beegfs.admon.gui.components.listeners.RepaintAdjustmentListener;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.components.menus.JTreeMenu;
import com.beegfs.admon.gui.components.panels.StatusPanel;
import com.beegfs.admon.gui.program.Main;
import java.awt.Dimension;
import java.beans.PropertyVetoException;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JDesktopPane;
import javax.swing.JInternalFrame;
import javax.swing.JOptionPane;


public class MainWindow extends javax.swing.JFrame
{
   static final Logger LOGGER = Logger.getLogger(MainWindow.class.getCanonicalName());
   private static final long serialVersionUID = 1L;

   private static final int LOGO_WIDHT = 133;
   private static final int LOGO_HEIGHT = 200;

   private transient final NodeEnvironment nodes;

   private final JDialogLogin loginDialog;
   private JTreeMenu treeMenu;

   public JDesktopPane getDesktopPane()
   {
      return jDesktopPaneContent;
   }

   private void showNotLoggedIn()
   {
      JOptionPane.showMessageDialog(null, Main.getLocal().getString("You must be logged in to view this information!"),
         Main.getLocal().getString("Not authenticated"), JOptionPane.ERROR_MESSAGE);
   }

   private void showNotAdmin()
   {
      JOptionPane.showMessageDialog(null, Main.getLocal().getString("You must be logged in with an adminstrative account to ")
         + Main.getLocal().getString("view this information!"), Main.getLocal().getString("Not authenticated"), JOptionPane.ERROR_MESSAGE);
   }

   public StatusPanel getStatusPanel()
   {
      return jPanelStatus;
   }

   public void setLoggedIn()
   {
      setLoggedIn(false);
   }

   public void setLoggedIn(boolean admin)
   {
    this.setTitle(Main.getLocal().getString("BeeGFS admon @ ") + Main.getConfig().getAdmonHost() + ":" +
              Main.getConfig().getAdmonHttpPort());
      jMenuItemLogin.setText(Main.getLocal().getString("Logout"));
      if (admin)
      {
    	  jLabelUser.setText(Main.getLocal().getString("Administrator"));
         activateAdminMenus();
         jLabelQuickAdmin.setVisible(false);
         jPasswordFieldQuickAdmin.setVisible(false);
      }
      else
      {
         jLabelUser.setText("Information");
         jLabelQuickAdmin.setVisible(true);
         jPasswordFieldQuickAdmin.setVisible(true);
      }
      openKnownProblemsFrame();
      openMenu();
   }

   private void activateAdminMenus()
   {
      jMenuAdministration.setVisible(true);
   }

   public JDialogLogin getLoginDialog()
   {
      return this.loginDialog;
   }

   /**
    * Creates new form MainWindow
    */
   public MainWindow()
   {
      this.nodes = new NodeEnvironment();

      initComponents();

      this.setTitle(Main.getLocal().getString("BeeGFS admon"));
      this.getContentPane().setBackground(BeegfsLookAndFeel.getMenuBackground());
      jDesktopPaneContent.setBackground(java.awt.Color.WHITE);
      jScrollPaneDesktop.getHorizontalScrollBar().addAdjustmentListener(
         new RepaintAdjustmentListener(jDesktopPaneContent));
      jScrollPaneDesktop.getVerticalScrollBar().addAdjustmentListener(
         new RepaintAdjustmentListener(jDesktopPaneContent));
      this.setExtendedState(this.getExtendedState() | MainWindow.MAXIMIZED_BOTH);

      jPanelMenue.setBackground(BeegfsLookAndFeel.getMenuBackground());
      jPanelMenuParent.setBackground(BeegfsLookAndFeel.getMenuBackground());
      jLabelQuickAdmin.setVisible(false);
      jPasswordFieldQuickAdmin.setVisible(false);
      jMenuAdministration.setVisible(false);
      jMenuWindow.setVisible(false);

      this.setIconImage(GuiTk.getFrameIcon().getImage());

      this.loginDialog = new JDialogLogin(this, true);
   }

   public void updateInternalDesktopResolution(int resolutionX, int resolutionY)
   {
      if (this.jLabelUserText != null)
      {
         this.jPanelDesktop.setSize(new Dimension(resolutionX, resolutionY));
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

      jToolBar = new javax.swing.JToolBar();
      jPanelToolBar = new javax.swing.JPanel();
      jLabelUserText = new javax.swing.JLabel();
      jLabelUser = new javax.swing.JLabel();
      jLabelQuickAdmin = new javax.swing.JLabel();
      jPasswordFieldQuickAdmin = new javax.swing.JPasswordField();
      jPanelStatus = new com.beegfs.admon.gui.components.panels.StatusPanel();
      jSplitPaneCenter = new javax.swing.JSplitPane();
      jPanelMenue = new javax.swing.JPanel();
      jLabelTopLogo = new javax.swing.JLabel();
      jScrollPaneMenue = new javax.swing.JScrollPane();
      jPanelMenuParent = new javax.swing.JPanel();
      jScrollPaneDesktop = new javax.swing.JScrollPane();
      jPanelDesktop = new javax.swing.JPanel();
      jDesktopPaneContent = new javax.swing.JDesktopPane();
      jMenuBar = new javax.swing.JMenuBar();
      jMenuAdmon = new javax.swing.JMenu();
      jMenuItemConnSettings = new javax.swing.JMenuItem();
      jMenuItemLogin = new javax.swing.JMenuItem();
      jMenuItemClose = new javax.swing.JMenuItem();
      jMenuAdministration = new javax.swing.JMenu();
      jMenuItemUserSettings = new javax.swing.JMenuItem();
      jMenuItemMailSettings = new javax.swing.JMenuItem();
      jMenuWindow = new javax.swing.JMenu();
      jMenuItemTile = new javax.swing.JMenuItem();
      jMenuItemMinimize = new javax.swing.JMenuItem();
      jMenuAbout = new javax.swing.JMenu();
      JMenuItemAbout = new javax.swing.JMenuItem();

      setDefaultCloseOperation(javax.swing.WindowConstants.EXIT_ON_CLOSE);
      setTitle("BeeGFS admon client");
      setBackground(new java.awt.Color(0, 0, 0));
      setName("mainWindow"); // NOI18N
      addWindowListener(new java.awt.event.WindowAdapter()
      {
         public void windowOpened(java.awt.event.WindowEvent evt)
         {
            formWindowOpened(evt);
         }
         public void windowClosed(java.awt.event.WindowEvent evt)
         {
            formWindowClosed(evt);
         }
      });

      jToolBar.setRollover(true);

      jLabelUserText.setText(Main.getLocal().getString("Currently logged in : "));

      jLabelQuickAdmin.setText(Main.getLocal().getString("Administrative Login : "));

      jPasswordFieldQuickAdmin.addActionListener(new java.awt.event.ActionListener()
      {
         public void actionPerformed(java.awt.event.ActionEvent evt)
         {
            jPasswordFieldQuickAdminActionPerformed(evt);
         }
      });

      javax.swing.GroupLayout jPanelToolBarLayout = new javax.swing.GroupLayout(jPanelToolBar);
      jPanelToolBar.setLayout(jPanelToolBarLayout);
      jPanelToolBarLayout.setHorizontalGroup(
         jPanelToolBarLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelToolBarLayout.createSequentialGroup()
            .addContainerGap()
            .addComponent(jLabelUserText)
            .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
            .addGroup(jPanelToolBarLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.TRAILING)
               .addGroup(jPanelToolBarLayout.createSequentialGroup()
                  .addComponent(jLabelUser)
                  .addContainerGap(855, Short.MAX_VALUE))
               .addGroup(jPanelToolBarLayout.createSequentialGroup()
                  .addComponent(jLabelQuickAdmin)
                  .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                  .addComponent(jPasswordFieldQuickAdmin, javax.swing.GroupLayout.PREFERRED_SIZE, 132, javax.swing.GroupLayout.PREFERRED_SIZE))))
      );
      jPanelToolBarLayout.setVerticalGroup(
         jPanelToolBarLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelToolBarLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE, false)
            .addComponent(jLabelUserText, javax.swing.GroupLayout.PREFERRED_SIZE, 21, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addComponent(jLabelUser)
            .addComponent(jLabelQuickAdmin)
            .addComponent(jPasswordFieldQuickAdmin, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
      );

      jToolBar.add(jPanelToolBar);

      jSplitPaneCenter.setDividerSize(3);

      jPanelMenue.setMinimumSize(new java.awt.Dimension(250, 200));
      jPanelMenue.setName(""); // NOI18N
      jPanelMenue.setPreferredSize(new java.awt.Dimension(200, 800));

      jLabelTopLogo.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
      jLabelTopLogo.setIcon(GuiTk.resizeImage(new javax.swing.ImageIcon(MainWindow.class.getResource(FilePathsEnum.IMAGE_BEEGFS_LOGO.getPath())), LOGO_WIDHT, LOGO_HEIGHT));

      jPanelMenuParent.setLayout(new java.awt.GridLayout(1, 0));
      jScrollPaneMenue.setViewportView(jPanelMenuParent);

      javax.swing.GroupLayout jPanelMenueLayout = new javax.swing.GroupLayout(jPanelMenue);
      jPanelMenue.setLayout(jPanelMenueLayout);
      jPanelMenueLayout.setHorizontalGroup(
         jPanelMenueLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelMenueLayout.createSequentialGroup()
            .addContainerGap()
            .addGroup(jPanelMenueLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
               .addComponent(jLabelTopLogo, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
               .addComponent(jScrollPaneMenue))
            .addContainerGap())
      );
      jPanelMenueLayout.setVerticalGroup(
         jPanelMenueLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(jPanelMenueLayout.createSequentialGroup()
            .addContainerGap()
            .addComponent(jLabelTopLogo, javax.swing.GroupLayout.PREFERRED_SIZE, 150, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.UNRELATED)
            .addComponent(jScrollPaneMenue, javax.swing.GroupLayout.DEFAULT_SIZE, 626, Short.MAX_VALUE))
      );

      jSplitPaneCenter.setLeftComponent(jPanelMenue);

      jPanelDesktop.setPreferredSize(new Dimension(Main.getConfig().getResolutionX(),
         Main.getConfig().getResolutionY()));

   jDesktopPaneContent.setPreferredSize(new java.awt.Dimension(33, 33));
   jDesktopPaneContent.addComponentListener(new java.awt.event.ComponentAdapter()
   {
      public void componentResized(java.awt.event.ComponentEvent evt)
      {
         jDesktopPaneContentComponentResized(evt);
      }
      public void componentShown(java.awt.event.ComponentEvent evt)
      {
         jDesktopPaneContentComponentShown(evt);
      }
   });
   com.beegfs.admon.gui.components.managers.DesktopManager desktopManager = new com.beegfs.admon.gui.components.managers.DesktopManager(jDesktopPaneContent);
   jDesktopPaneContent.setDesktopManager(desktopManager);

   javax.swing.GroupLayout jPanelDesktopLayout = new javax.swing.GroupLayout(jPanelDesktop);
   jPanelDesktop.setLayout(jPanelDesktopLayout);
   jPanelDesktopLayout.setHorizontalGroup(
      jPanelDesktopLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
      .addComponent(jDesktopPaneContent, javax.swing.GroupLayout.DEFAULT_SIZE, 996, Short.MAX_VALUE)
   );
   jPanelDesktopLayout.setVerticalGroup(
      jPanelDesktopLayout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
      .addComponent(jDesktopPaneContent, javax.swing.GroupLayout.Alignment.TRAILING, javax.swing.GroupLayout.DEFAULT_SIZE, 798, Short.MAX_VALUE)
   );

   jScrollPaneDesktop.setViewportView(jPanelDesktop);

   jSplitPaneCenter.setRightComponent(jScrollPaneDesktop);

   jMenuBar.setBorder(new javax.swing.border.SoftBevelBorder(javax.swing.border.BevelBorder.RAISED));

   jMenuAdmon.setText(Main.getLocal().getString("Admon"));

   jMenuItemConnSettings.setText(Main.getLocal().getString("Change Settings"));
   jMenuItemConnSettings.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemConnSettingsActionPerformed(evt);
      }
   });
   jMenuAdmon.add(jMenuItemConnSettings);

   jMenuItemLogin.setText(Main.getLocal().getString("Login"));
   jMenuItemLogin.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemLoginActionPerformed(evt);
      }
   });
   jMenuAdmon.add(jMenuItemLogin);

   jMenuItemClose.setText(Main.getLocal().getString("Close"));
   jMenuItemClose.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemCloseActionPerformed(evt);
      }
   });
   jMenuAdmon.add(jMenuItemClose);

   jMenuBar.add(jMenuAdmon);

   jMenuAdministration.setText(Main.getLocal().getString("Administration"));

   jMenuItemUserSettings.setText(Main.getLocal().getString("User Settings"));
   jMenuItemUserSettings.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemUserSettingsActionPerformed(evt);
      }
   });
   jMenuAdministration.add(jMenuItemUserSettings);

   jMenuItemMailSettings.setText(Main.getLocal().getString("Mail Settings"));
   jMenuItemMailSettings.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemMailSettingsActionPerformed(evt);
      }
   });
   jMenuAdministration.add(jMenuItemMailSettings);

   jMenuBar.add(jMenuAdministration);

   jMenuWindow.setText(Main.getLocal().getString("Windows"));

   jMenuItemTile.setText(Main.getLocal().getString("Tile"));
   jMenuItemTile.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemTileActionPerformed(evt);
      }
   });
   jMenuWindow.add(jMenuItemTile);

   jMenuItemMinimize.setText(Main.getLocal().getString("Minimize All"));
   jMenuItemMinimize.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuItemMinimizeActionPerformed(evt);
      }
   });
   jMenuWindow.add(jMenuItemMinimize);

   jMenuBar.add(jMenuWindow);

   jMenuAbout.setText(Main.getLocal().getString("About"));
   jMenuAbout.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         jMenuAboutActionPerformed(evt);
      }
   });

   JMenuItemAbout.setText(Main.getLocal().getString("About"));
   JMenuItemAbout.addActionListener(new java.awt.event.ActionListener()
   {
      public void actionPerformed(java.awt.event.ActionEvent evt)
      {
         JMenuItemAboutActionPerformed(evt);
      }
   });
   jMenuAbout.add(JMenuItemAbout);

   jMenuBar.add(jMenuAbout);

   setJMenuBar(jMenuBar);

   javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
   getContentPane().setLayout(layout);
   layout.setHorizontalGroup(
      layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
      .addComponent(jSplitPaneCenter, javax.swing.GroupLayout.DEFAULT_SIZE, 1047, Short.MAX_VALUE)
      .addGroup(layout.createSequentialGroup()
         .addContainerGap()
         .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addComponent(jToolBar, javax.swing.GroupLayout.DEFAULT_SIZE, 1023, Short.MAX_VALUE)
            .addComponent(jPanelStatus, javax.swing.GroupLayout.DEFAULT_SIZE, 1023, Short.MAX_VALUE))
         .addContainerGap())
   );
   layout.setVerticalGroup(
      layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
      .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, layout.createSequentialGroup()
         .addComponent(jSplitPaneCenter)
         .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
         .addComponent(jToolBar, javax.swing.GroupLayout.PREFERRED_SIZE, 25, javax.swing.GroupLayout.PREFERRED_SIZE)
         .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
         .addComponent(jPanelStatus, javax.swing.GroupLayout.PREFERRED_SIZE, 59, javax.swing.GroupLayout.PREFERRED_SIZE)
         .addContainerGap())
   );

   pack();
   }// </editor-fold>//GEN-END:initComponents

    private void jMenuItemCloseActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemCloseActionPerformed
      this.setVisible(false);
      this.dispose();
    }//GEN-LAST:event_jMenuItemCloseActionPerformed

    private void formWindowClosed(java.awt.event.WindowEvent evt) {//GEN-FIRST:event_formWindowClosed
      treeMenu.stopUpdateMenuThread();
      nodes.stopUpdateThread();
    }//GEN-LAST:event_formWindowClosed

    private void jMenuItemLoginActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemLoginActionPerformed
      if (Main.getSession().getIsInfo())
      {
    	 this.setTitle(Main.getLocal().getString("BeeGFS admon"));
         Main.getSession().setIsInfo(false);
         Main.getSession().setIsAdmin(false);
         jMenuItemLogin.setText(Main.getLocal().getString("Login"));
         loginDialog.setVisible(true);
      }
      else
      {
         loginDialog.setVisible(true);
      }
    }//GEN-LAST:event_jMenuItemLoginActionPerformed

    private void jMenuItemUserSettingsActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemUserSettingsActionPerformed
      if (Main.getSession().getIsAdmin())
      {
         javax.swing.JInternalFrame frame = new JInternalFrameUserSettings();
         if (!FrameManager.isFrameOpen((JInternalFrameInterface) frame, true))
         {
            jDesktopPaneContent.add(frame);
            frame.setVisible(true);
            FrameManager.addFrame((JInternalFrameInterface) frame);
         }
      }
      else
      {
         showNotAdmin();
      }
    }//GEN-LAST:event_jMenuItemUserSettingsActionPerformed

    private void jMenuItemMailSettingsActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemMailSettingsActionPerformed
      if (Main.getSession().getIsAdmin())
      {
         javax.swing.JInternalFrame frame = new JInternalFrameNotifications();
         if (!FrameManager.isFrameOpen((JInternalFrameInterface) frame, true))
         {
            jDesktopPaneContent.add(frame);
            frame.setVisible(true);
            FrameManager.addFrame((JInternalFrameInterface) frame);
         }
      }
      else
      {
         showNotAdmin();
      }
    }//GEN-LAST:event_jMenuItemMailSettingsActionPerformed

    private void jPasswordFieldQuickAdminActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jPasswordFieldQuickAdminActionPerformed
      try
      {
         String username = "Administrator";
         String pw = CryptTk.getMD5(jPasswordFieldQuickAdmin.getPassword());
         if (!loginDialog.doLogin(username, pw))
         {
            JOptionPane.showMessageDialog(this, Main.getLocal().getString("Authentication as Administrator failed"),
               Main.getLocal().getString("Authentication failed"), JOptionPane.ERROR_MESSAGE);
         }
      }
      catch (WrongBackendVersionException e)
      {
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Wrong backend version"), e);
      }
      catch (CommunicationException e)
      {
         JOptionPane.showMessageDialog(Main.getMainWindow(), Main.getLocal().getString("Unable to communicate with the ")
            + Main.getLocal().getString("remote backend."), Main.getLocal().getString("Communication Error"), JOptionPane.ERROR_MESSAGE);
         LOGGER.log(Level.SEVERE, Main.getLocal().getString("Communication Error: Unable to communicate with the ")
            + Main.getLocal().getString("remote backend."), e);
      }
    }//GEN-LAST:event_jPasswordFieldQuickAdminActionPerformed

    private void jDesktopPaneContentComponentResized(java.awt.event.ComponentEvent evt) {//GEN-FIRST:event_jDesktopPaneContentComponentResized
       // get all frames back into the desktop
        /*
        * for (JInternalFrame f : jDesktopPaneContent.getAllFrames()) { Rectangle bounds =
        * f.getBounds(); if ((bounds.x + bounds.getWidth()) >
        * jDesktopPaneContent.getSize().getWidth()) { bounds.x = (int)
        * (jDesktopPaneContent.getSize().getWidth() - bounds.getWidth() - 1); f.setBounds(bounds); }
        * if ((bounds.y + bounds.getHeight()) > jDesktopPaneContent.getSize().getHeight()) {
        * bounds.y = (int) (jDesktopPaneContent.getSize().getHeight() - bounds.getHeight() - 1);
        * f.setBounds(bounds); } if (f.getWidth() > jDesktopPaneContent.getWidth()) {
        * f.setPreferredSize(new Dimension(jDesktopPaneContent.getWidth() - 1, f.getHeight()));
        * f.setSize(jDesktopPaneContent.getWidth() - 1, f.getHeight()); } if (f.getHeight() >
        * jDesktopPaneContent.getHeight()) { f.setPreferredSize(new Dimension(f.getWidth(),
        * jDesktopPaneContent.getHeight() - 1)); f.setSize(f.getWidth(),
        * jDesktopPaneContent.getHeight() - 1); }
        *
        * }
        */
    }//GEN-LAST:event_jDesktopPaneContentComponentResized

   private void openKnownProblemsFrame()
   {
      if (Main.getSession().getIsInfo())
      {
         JInternalFrameKnownProblems frame = new JInternalFrameKnownProblems();
         if (!FrameManager.isFrameOpen(frame, true))
         {
            jDesktopPaneContent.add(frame);
            frame.reshape(400, 500, frame.getWidth(), frame.getHeight());
            frame.setVisible(true);
            FrameManager.addFrame(frame);
         }
      }
      else
      {
         showNotLoggedIn();
      }
   }

   private void openMenu()
   {
      if (Main.getSession().getIsInfo())
      {
         jPanelMenuParent.removeAll();
         jPanelMenuParent.invalidate();
         treeMenu = new JTreeMenu(nodes);
         treeMenu.startUpdateMenuThread();
         jPanelMenuParent.add(treeMenu);
         treeMenu.setVisible(true);
      }
      else
      {
         showNotLoggedIn();
      }
   }

    private void jDesktopPaneContentComponentShown(java.awt.event.ComponentEvent evt) {//GEN-FIRST:event_jDesktopPaneContentComponentShown
    }//GEN-LAST:event_jDesktopPaneContentComponentShown

    private void formWindowOpened(java.awt.event.WindowEvent evt) {//GEN-FIRST:event_formWindowOpened
      nodes.startUpdateThread();
      toFront();
    }//GEN-LAST:event_formWindowOpened

    private void jMenuItemConnSettingsActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemConnSettingsActionPerformed
      JDialogNewConfig newConfig = new JDialogNewConfig(null, true);
      newConfig.setVisible(true);
    }//GEN-LAST:event_jMenuItemConnSettingsActionPerformed

    private void jMenuItemMinimizeActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemMinimizeActionPerformed
      JInternalFrame[] frames = jDesktopPaneContent.getAllFrames();
      for (JInternalFrame f : frames)
      {
         try
         {
            f.setIcon(true);
         }
         catch (PropertyVetoException ex)
         {
            LOGGER.log(Level.FINEST, "Internal error.", ex);
         }
      }
    }//GEN-LAST:event_jMenuItemMinimizeActionPerformed

    private void jMenuAboutActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuAboutActionPerformed
    }//GEN-LAST:event_jMenuAboutActionPerformed

    private void JMenuItemAboutActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_JMenuItemAboutActionPerformed
      JDialogAbout d = new JDialogAbout(this, true);
      d.setVisible(true);
    }//GEN-LAST:event_JMenuItemAboutActionPerformed

    private void jMenuItemTileActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_jMenuItemTileActionPerformed
      tile(true);
    }//GEN-LAST:event_jMenuItemTileActionPerformed

   public void tile(boolean tile)
   {
      JInternalFrame[] frames = jDesktopPaneContent.getAllFrames();
      if (tile)
      {
         int frameCount = frames.length;
         int width = jDesktopPaneContent.getWidth() / 2;
         int height = jDesktopPaneContent.getHeight() / ((frameCount + 1) / 2);
         for (int i = 0; i < frameCount; i++)
         {
            JInternalFrame f = frames[i];
            try
            {
               f.setIcon(false);
            }
            catch (PropertyVetoException ex)
            {
               LOGGER.log(Level.FINEST, "Internal error", ex);
            }

            if (i % 2 == 0)
            {
               f.reshape(0, height * (i / 2), width, height);
            }
            else
            {
               f.reshape(width, (height * (i / 2)), width, height);
            }
         }
      }
      else
      {
         for (JInternalFrame f : frames)
         {
            f.reshape(f.getX(), f.getY(), f.getWidth() - 20, f.getHeight() - 20);
         }
      }
   }
   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JMenuItem JMenuItemAbout;
   private javax.swing.JDesktopPane jDesktopPaneContent;
   private javax.swing.JLabel jLabelQuickAdmin;
   private javax.swing.JLabel jLabelTopLogo;
   private javax.swing.JLabel jLabelUser;
   private javax.swing.JLabel jLabelUserText;
   private javax.swing.JMenu jMenuAbout;
   private javax.swing.JMenu jMenuAdministration;
   private javax.swing.JMenu jMenuAdmon;
   private javax.swing.JMenuBar jMenuBar;
   private javax.swing.JMenuItem jMenuItemClose;
   private javax.swing.JMenuItem jMenuItemConnSettings;
   private javax.swing.JMenuItem jMenuItemLogin;
   private javax.swing.JMenuItem jMenuItemMailSettings;
   private javax.swing.JMenuItem jMenuItemMinimize;
   private javax.swing.JMenuItem jMenuItemTile;
   private javax.swing.JMenuItem jMenuItemUserSettings;
   private javax.swing.JMenu jMenuWindow;
   private javax.swing.JPanel jPanelDesktop;
   private javax.swing.JPanel jPanelMenuParent;
   private javax.swing.JPanel jPanelMenue;
   private com.beegfs.admon.gui.components.panels.StatusPanel jPanelStatus;
   private javax.swing.JPanel jPanelToolBar;
   private javax.swing.JPasswordField jPasswordFieldQuickAdmin;
   private javax.swing.JScrollPane jScrollPaneDesktop;
   private javax.swing.JScrollPane jScrollPaneMenue;
   private javax.swing.JSplitPane jSplitPaneCenter;
   private javax.swing.JToolBar jToolBar;
   // End of variables declaration//GEN-END:variables
}

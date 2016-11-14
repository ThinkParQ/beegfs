package com.beegfs.admon.gui.components.menus;

import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_METADATA;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_STORAGE;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_USER_METADATA;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_USER_STORAGE;
import com.beegfs.admon.gui.common.nodes.Node;
import com.beegfs.admon.gui.common.nodes.NodeEnvironment;
import com.beegfs.admon.gui.common.nodes.Nodes;
import com.beegfs.admon.gui.common.threading.GuiThread;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.internalframes.install.JInternalFrameInstall;
import com.beegfs.admon.gui.components.internalframes.install.JInternalFrameInstallationConfig;
import com.beegfs.admon.gui.components.internalframes.install.JInternalFrameUninstall;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameFileBrowser;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameKnownProblems;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameLogFile;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameRemoteLogFiles;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameStartStop;
import com.beegfs.admon.gui.components.internalframes.management.JInternalFrameStriping;
import com.beegfs.admon.gui.components.internalframes.stats.JInternalFrameMetaNode;
import com.beegfs.admon.gui.components.internalframes.stats.JInternalFrameMetaNodesOverview;
import com.beegfs.admon.gui.components.internalframes.stats.JInternalFrameStats;
import com.beegfs.admon.gui.components.internalframes.stats.JInternalFrameStorageNode;
import com.beegfs.admon.gui.components.internalframes.stats.JInternalFrameStorageNodesOverview;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.program.Main;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.util.Iterator;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JInternalFrame;
import javax.swing.JTree;
import javax.swing.border.EtchedBorder;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeModel;
import javax.swing.tree.MutableTreeNode;
import javax.swing.tree.TreeNode;
import javax.swing.tree.TreePath;

public class JTreeMenu extends JTree
{
   static final Logger LOGGER = Logger.getLogger(
      JTreeMenu.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "MenuNodeList";

   private transient final NodeEnvironment nodes;

   private final Lock updateLock;
   private final transient Condition newData;
   private final transient MenuUpdateThread updateThread;

   public JTreeMenu(NodeEnvironment newNodes)
   {
      TreeMenuCellRenderer menuCellRenderer = new TreeMenuCellRenderer();
      this.setCellRenderer(menuCellRenderer);
      this.setEnabled(true);
      this.setOpaque(true);
      this.setDoubleBuffered(true);
      this.setBorder(new EtchedBorder());

      this.addMouseListener(new TreeMenuMouseListener());

      this.nodes = newNodes;

      DefaultTreeModel model = (DefaultTreeModel) this.getModel();
      DefaultMutableTreeNode topNode = new DefaultMutableTreeNode("Menu");
      model.setRoot(topNode);

      rebuildTree();

      updateLock = this.nodes.getUpdateLock();
      newData = this.nodes.getNewDataCondition();
      updateThread = new MenuUpdateThread(THREAD_NAME);
   }

   final public boolean updateMetadataNodes()
   {
      try
      {
         Nodes newNodes = this.nodes.getNodes(NodeTypesEnum.METADATA).getClonedNodes(
            Node.DEFAULT_GROUP);

         DefaultTreeModel model = (DefaultTreeModel) this.getModel();
         DefaultMutableTreeNode rootNode = (DefaultMutableTreeNode)model.getRoot();
         DefaultMutableTreeNode parentNode = null;

         for (int rootIndex = 0; rootIndex < rootNode.getChildCount(); rootIndex++)
         {
            DefaultMutableTreeNode tmpNode = (DefaultMutableTreeNode)rootNode.getChildAt(rootIndex);
            if(tmpNode.toString().equalsIgnoreCase("Metadata nodes"))
            {
               for (int index = 0; index < tmpNode.getChildCount(); index++)
               {
                  if(tmpNode.getChildAt(index).toString().equalsIgnoreCase("Node details"))
                  {
                     parentNode = (DefaultMutableTreeNode)tmpNode.getChildAt(index);
                     break;
                  }
               }
            }
         }

         if(parentNode != null)
         {
            TreePath parentPath = new TreePath(parentNode.getPath());
            boolean isExpanded = this.isExpanded(parentPath);

            if (parentNode.getChildCount() > 0)
            {
               for (int i = parentNode.getChildCount() - 1; i >= 0; i--)
               {
                  DefaultMutableTreeNode n = (DefaultMutableTreeNode) parentNode.getChildAt(i);
                  model.removeNodeFromParent(n);
               }
            }

            for (Iterator<Node> iter = newNodes.iterator(); iter.hasNext();)
            {
               final Node tmpNode = iter.next();
               model.insertNodeInto(new DefaultMutableTreeNode(tmpNode.getTypedNodeID()),
                  parentNode, parentNode.getChildCount());
            }

            if (isExpanded)
            {
               this.expandPath(parentPath);
            }
         }
         return true;
      }
      catch (CloneNotSupportedException e)
      {
         LOGGER.log(Level.FINEST, "Internal error.", e);
         return false;
      }
   }

   final public boolean updateStorageNodes()
   {
      try
      {
         Nodes newNodes = this.nodes.getNodes(NodeTypesEnum.STORAGE).getClonedNodes(
            Node.DEFAULT_GROUP);

         DefaultTreeModel model = (DefaultTreeModel) this.getModel();
         DefaultMutableTreeNode rootNode = (DefaultMutableTreeNode)model.getRoot();
         DefaultMutableTreeNode parentNode = null;

         for (int rootIndex = 0; rootIndex < rootNode.getChildCount(); rootIndex++)
         {
            DefaultMutableTreeNode tmpNode = (DefaultMutableTreeNode)rootNode.getChildAt(rootIndex);
            if(tmpNode.toString().equalsIgnoreCase("Storage nodes"))
            {
               for (int index = 0; index < tmpNode.getChildCount(); index++)
               {
                  if(tmpNode.getChildAt(index).toString().equalsIgnoreCase("Node details"))
                  {
                     parentNode = (DefaultMutableTreeNode)tmpNode.getChildAt(index);
                     break;
                  }
               }
            }
         }

         if(parentNode != null)
         {
            TreePath parentPath = new TreePath(parentNode.getPath());
            boolean isExpanded = this.isExpanded(parentPath);

            if (parentNode.getChildCount() > 0)
            {
               for (int i = parentNode.getChildCount() - 1; i >= 0; i--)
               {
                  DefaultMutableTreeNode n = (DefaultMutableTreeNode) parentNode.getChildAt(i);
                  model.removeNodeFromParent(n);
               }
            }

            for (Iterator<Node> iter = newNodes.iterator(); iter.hasNext();)
            {
               final Node tmpNode = iter.next();
               model.insertNodeInto(new DefaultMutableTreeNode(tmpNode.getTypedNodeID() ),
                  (MutableTreeNode) parentPath.getLastPathComponent(),
                  ((TreeNode) parentPath.getLastPathComponent()).getChildCount() );
            }

            if (isExpanded)
            {
               this.expandPath(parentPath);
            }
         }
         
         return true;
      }
      catch (java.lang.NullPointerException | CloneNotSupportedException e)
      {
         LOGGER.log(Level.FINEST, "Internal error.", e);
         return false;
      }
   }

   private boolean openFrame(TreePath path)
   {
      JInternalFrame frame;

      // make the right frame
      Object[] pathObjects = path.getPath();
      int pathCount = path.getPathCount();

      String submenu = pathObjects[1].toString();
      String item = pathObjects[pathCount - 1].toString();
      int nodeNumID = 0;

      try
      {
         nodeNumID = Node.getNodeNumIDFromTypedNodeID(item);
      }
      catch (NumberFormatException e)
      {
         LOGGER.log(Level.FINEST, "Couldn't parse NodeNumID from string: {0}", item);
      }

      // First check the submenu we are in, especially if we are in meta or storage nodes
      if (submenu.equalsIgnoreCase("metadata nodes") && (Main.getSession().getIsInfo()))
      {
         if (item.equalsIgnoreCase("overview"))
         {
            frame = new JInternalFrameMetaNodesOverview();
         }
         else
         if (!item.equalsIgnoreCase("Node details"))
         {
            frame = new JInternalFrameMetaNode(this.nodes.getNode(
               NodeTypesEnum.METADATA, nodeNumID));
         }
         else
         {
            return true;
         }
      }
      else if (submenu.equalsIgnoreCase("storage nodes") && (Main.getSession().getIsInfo()))
      {
         if (item.equalsIgnoreCase("overview"))
         {
            frame = new JInternalFrameStorageNodesOverview();
         }
         else
         if (!item.equalsIgnoreCase("Node details"))
         {
            frame = new JInternalFrameStorageNode(this.nodes.getNode(
               NodeTypesEnum.STORAGE, nodeNumID));
         }
         else
         {
            return true;
         }
      }
      else if (submenu.equalsIgnoreCase("Client statistics") && (Main.getSession().getIsInfo()))
      {
         if (item.equalsIgnoreCase("metadata") )
         {
            frame = new JInternalFrameStats(STATS_CLIENT_METADATA);
         }
         else if (item.equalsIgnoreCase("storage") )
         {
            frame = new JInternalFrameStats(STATS_CLIENT_STORAGE);
         }
         else
         {
            return true;
         }
      }
      else if (submenu.equalsIgnoreCase("User statistics") && (Main.getSession().getIsInfo()))
      {
         if (item.equalsIgnoreCase("metadata") )
         {
            frame = new JInternalFrameStats(STATS_USER_METADATA);
         }
         else if (item.equalsIgnoreCase("storage") )
         {
            frame = new JInternalFrameStats(STATS_USER_STORAGE);
         }
         else
         {
            return true;
         }
      }
      /*
      else if (submenu.equalsIgnoreCase("Quota") && (Main.getSession().getIsInfo()))
      {
         if (item.equalsIgnoreCase("Show Quota") )
         {
            frame = new JInternalFrameShowQuota();
         }
         else if (item.equalsIgnoreCase("Set Quota Limits") )
         {
            frame = new JInternalFrameSetQuota();
         }
         else
         {
            return true;
         }
      } */ // if not we can directly have a look at the last par
      else if (item.equalsIgnoreCase("known problems") && (Main.getSession().getIsInfo()))
      {
         frame = new JInternalFrameKnownProblems();
      }
      else if (item.equalsIgnoreCase("stripe settings") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameStriping();
      }
      else if (item.equalsIgnoreCase("file browser") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameFileBrowser();
      }
      else if (item.equalsIgnoreCase("configuration") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameInstallationConfig();
      }
      else if (item.equalsIgnoreCase("install") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameInstall();
      }
      else if (item.equalsIgnoreCase("uninstall") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameUninstall();
      }
      else if (item.equalsIgnoreCase("start/stop daemon") &&
         (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameStartStop();
      }
      else if (item.equalsIgnoreCase("view log file") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameRemoteLogFiles(nodes);
      }
      else if (item.equalsIgnoreCase("installation log file") && (Main.getSession().getIsAdmin()))
      {
         frame = new JInternalFrameLogFile();
      }
      else
      {
         return false;
      }

      if (!FrameManager.isFrameOpen((JInternalFrameInterface) frame, true)) {
         Main.getMainWindow().getDesktopPane().add(frame);
         frame.setVisible(true);
         FrameManager.addFrame((JInternalFrameInterface) frame);
      }

      return true;
   }

   final public void rebuildTree()
   {
      // delete everything
      TreePath topPath = this.getNextMatch("Menu", 0, null);
      DefaultMutableTreeNode topNode = (DefaultMutableTreeNode) topPath.getLastPathComponent();
      DefaultTreeModel model = (DefaultTreeModel) this.getModel();

      for (int i = topNode.getChildCount() - 1; i >= 0; i--)
      {
         DefaultMutableTreeNode n = (DefaultMutableTreeNode) topNode.getChildAt(i);
         model.removeNodeFromParent(n);
      }
 
      DefaultMutableTreeNode menuNode = new DefaultMutableTreeNode("Metadata nodes");
      model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
      model.insertNodeInto(new DefaultMutableTreeNode("Overview"), menuNode, menuNode.
         getChildCount());
      model.insertNodeInto(new DefaultMutableTreeNode("Node details"), menuNode, menuNode.
         getChildCount());

      menuNode = new DefaultMutableTreeNode("Storage nodes");
      model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
      model.insertNodeInto(new DefaultMutableTreeNode("Overview"), menuNode, menuNode.
         getChildCount());
      model.insertNodeInto(new DefaultMutableTreeNode("Node details"), menuNode, menuNode.
         getChildCount());

      menuNode = new DefaultMutableTreeNode("Client statistics");
      model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
      model.insertNodeInto(new DefaultMutableTreeNode("Metadata"), menuNode, menuNode.
         getChildCount());
      model.
         insertNodeInto(new DefaultMutableTreeNode("Storage"), menuNode, menuNode.getChildCount());

      menuNode = new DefaultMutableTreeNode("User statistics");
      model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
      model.insertNodeInto(new DefaultMutableTreeNode("Metadata"), menuNode, menuNode.
         getChildCount());
      model.
         insertNodeInto(new DefaultMutableTreeNode("Storage"), menuNode, menuNode.getChildCount());

      /*
      menuNode = new DefaultMutableTreeNode("Quota");
      model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
      model.insertNodeInto(new DefaultMutableTreeNode("Show Quota"), menuNode, menuNode.
         getChildCount());
      model.insertNodeInto(new DefaultMutableTreeNode("Set Quota Limits"), menuNode, menuNode
         .getChildCount());
      */

      menuNode = new DefaultMutableTreeNode("Management");
      model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
      model.insertNodeInto(new DefaultMutableTreeNode("Known Problems"), menuNode, menuNode.
         getChildCount());

      if (Main.getSession().getIsAdmin())
      {
         model.insertNodeInto(new DefaultMutableTreeNode("Start/Stop Daemon"), menuNode,
            menuNode.getChildCount());
         model.insertNodeInto(new DefaultMutableTreeNode("View Log File"), menuNode,
            menuNode.getChildCount());


         menuNode = new DefaultMutableTreeNode("FS Operations");
         model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
         model.insertNodeInto(new DefaultMutableTreeNode("Stripe Settings"), menuNode, menuNode.
            getChildCount());
         model.insertNodeInto(new DefaultMutableTreeNode("File Browser"), menuNode, menuNode.
            getChildCount());

         menuNode = new DefaultMutableTreeNode("Installation");
         model.insertNodeInto(menuNode, topNode, topNode.getChildCount() );
         model.insertNodeInto(new DefaultMutableTreeNode("Configuration"), menuNode, menuNode.
            getChildCount());
         model.insertNodeInto(new DefaultMutableTreeNode("Install"), menuNode, menuNode.
            getChildCount());
         model.insertNodeInto(new DefaultMutableTreeNode("Uninstall"), menuNode, menuNode.
            getChildCount());
         model.insertNodeInto(new DefaultMutableTreeNode("Installation Log File"), menuNode,
            menuNode.getChildCount());
      }
      
      this.expandPath(topPath);
   }

   public void startUpdateMenuThread()
   {
      updateThread.start();
   }

   public void stopUpdateMenuThread()
   {
      updateThread.shouldStop();
   }

   private class TreeMenuMouseListener extends MouseAdapter
   {
      @Override
      public void mousePressed(MouseEvent e)
      {
         JTreeMenu jTreeMenu = (JTreeMenu) e.getComponent();
         int selRow = jTreeMenu.getRowForLocation(e.getX(), e.getY());
         TreePath selPath = jTreeMenu.getPathForLocation(e.getX(), e.getY());
         if (selPath != null)
         {
            DefaultMutableTreeNode node = (DefaultMutableTreeNode)selPath.getLastPathComponent();
            if ((selRow != -1) && (node.isLeaf()))
            {
               if (e.getClickCount() >= 2)
               {
                  if (!openFrame(selPath))
                  {
                     LOGGER.log(Level.WARNING, "Tried to open frame " + selPath.toString() +
                        " which does not exist", false);
                  }
               }
            }
         }
      }
   }

   private class MenuUpdateThread extends GuiThread
   {
      MenuUpdateThread(String name)
      {
         super(name);
      }

      @Override
      public void run()
      {
         while(!stop)
         {
            updateLock.lock();
            try
            {
               if(!stop)
               {
                  updateMetadataNodes();
                  updateStorageNodes();
               }
               
               newData.await();
            }
            catch (InterruptedException ex)
            {
               LOGGER.log(Level.SEVERE, "Interrupted Exception in menu update thread run.", ex);
            }
            finally
            {
               updateLock.unlock();
            }
         }
      }
   }
}

package com.beegfs.admon.gui.components.internalframes.management;

import com.beegfs.admon.gui.common.ValueUnit;
import com.beegfs.admon.gui.common.XMLParser;
import com.beegfs.admon.gui.common.enums.SizeUnitEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.tools.GuiTk;
import com.beegfs.admon.gui.common.tools.HttpTk;
import com.beegfs.admon.gui.common.tools.UnitTk;
import com.beegfs.admon.gui.components.internalframes.JInternalFrameInterface;
import com.beegfs.admon.gui.components.managers.FrameManager;
import com.beegfs.admon.gui.components.tables.FileBrowserTableCellRenderer;
import com.beegfs.admon.gui.components.tables.FileBrowserTableCellRendererFilename;
import com.beegfs.admon.gui.components.tables.FileBrowserTableCellRendererStriping;
import com.beegfs.admon.gui.components.tables.FilenameObject;
import com.beegfs.admon.gui.program.Main;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.TreeMap;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

public class JInternalFrameFileBrowser extends javax.swing.JInternalFrame implements
        JInternalFrameInterface
{
   static final Logger LOGGER = Logger.getLogger(
      JInternalFrameFileBrowser.class.getCanonicalName());
   private static final long serialVersionUID = 1L;
   private static final String THREAD_NAME = "FileBrowser";

   private String currentPath;

   /**
    * Creates new form JInternalFrameMetaNode
    */
   public JInternalFrameFileBrowser()
   {
      initComponents();
      currentPath = "";
      setTitle(getFrameTitle());
      setFrameIcon(GuiTk.getFrameIcon());
      TableCellRenderer cellRenderer = new FileBrowserTableCellRendererFilename();
      TableColumn col = jTableFiles.getColumnModel().getColumn(0);
      col.setCellRenderer(cellRenderer);
      cellRenderer = new FileBrowserTableCellRenderer();
      int i = 1;
      while (i < 8)
      {
         col = jTableFiles.getColumnModel().getColumn(i);
         col.setCellRenderer(cellRenderer);
         i++;
      }
      col = jTableFiles.getColumnModel().getColumn(8);
      cellRenderer = new FileBrowserTableCellRendererStriping();
      col.setCellRenderer(cellRenderer);
      MouseListener mouseListener = new MouseListener()
      {

         @Override
         public void mouseClicked(MouseEvent e)
         {
            int row = jTableFiles.rowAtPoint(e.getPoint());
            int col = jTableFiles.columnAtPoint(e.getPoint());
            if (col == 0)
            {
               FilenameObject value = (FilenameObject) jTableFiles.getValueAt(row, col);
               if (value.isDirectory())
               {
                  String newPath;
                  if (value.getName().equals(".."))
                  {
                     int index = currentPath.lastIndexOf('/');
                     if (index > 0)
                     {
                        newPath = currentPath.substring(0, index);
                     }
                     else
                     {
                        newPath = "/";
                     }
                  }
                  else
                  {
                     newPath = currentPath + "/" + value.getName();
                  }
                  requestInfo(newPath);
               }
            }
            else
            if (col == 8)
            {
               FilenameObject value = (FilenameObject) jTableFiles.getValueAt(row, 0);
               String path;
               if (value.getName().equals(".."))
               {
                  int index = currentPath.lastIndexOf('/');
                  if (index > 0)
                  {
                     path = currentPath.substring(0, index);
                  }
                  else
                  {
                     path = "/";
                  }
               }
               else
               {
                  path = currentPath + "/" + value.getName();
               }
               JInternalFrameStriping frame = new JInternalFrameStriping();
               if (!FrameManager.isFrameOpen(frame, true))
               {
                  Main.getMainWindow().getDesktopPane().add(frame);
                  frame.setVisible(true);
                  FrameManager.addFrame(frame);
               }
               else
               {
                  frame = (JInternalFrameStriping) FrameManager.getOpenFrame(frame, true);
               }
               frame.setPathFromExternal(path, value.isDirectory());
            }
         }

         @Override
         public void mousePressed(MouseEvent arg0)
         {
         }

         @Override
         public void mouseReleased(MouseEvent arg0)
         {
         }

         @Override
         public void mouseEntered(MouseEvent arg0)
         {
         }

         @Override
         public void mouseExited(MouseEvent arg0)
         {
         }
      };

      jTableFiles.addMouseListener(mouseListener);
      jTableFiles.getRowSorter().toggleSortOrder(0);
      requestInfo("");
   }

   @Override
   public boolean isEqual(JInternalFrameInterface obj)
   {
      return obj instanceof JInternalFrameFileBrowser;
   }

   @SuppressWarnings("AssignmentToMethodParameter")
   private void requestInfo(String pathStr)
   {
      pathStr = pathStr.replace("//", "/");
      try
      {
         DefaultTableModel model = (DefaultTableModel) jTableFiles.getModel();
         for (int i = model.getRowCount() - 1; i >= 0; i--)
         {
            model.removeRow(i);
         }
         
         XMLParser parser = new XMLParser("" , THREAD_NAME);
         int totalFiles = 0;
         int totalFolder = 0;
         long totalSize = 0;
         long serverOffset = 0;
         boolean success;
         boolean moreToCome = true;
         FilenameObject o = new FilenameObject("..", true);
         Object[] row = new Object[]
         {
            o,
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            o
         };
         model.addRow(row);
         while (moreToCome)
         {
            String url = HttpTk.generateAdmonUrl("/XML_FileBrowser?pathStr=" + pathStr
                    + "&offset=" + serverOffset + "&entriesAtOnce=25");
            parser.setUrl(url);
            parser.update();
            
            success = Boolean.parseBoolean(parser.getValue("success"));
            if (success)
            {
               serverOffset = Long.parseLong(parser.getValue("offset", "general"));
               ArrayList<TreeMap<String, String>> entries =
                       parser.getVectorOfAttributeTreeMaps("entries");
               if (entries.isEmpty())
               {
                  moreToCome = false;
               }
               else
               {
                  Iterator<TreeMap<String, String>> iter = entries.iterator();
                  while (iter.hasNext())
                  {
                     try
                     {
                        TreeMap<String, String> entry = iter.next();
                        FilenameObject filenameObj = new FilenameObject(entry.get("value"),
                                Boolean.parseBoolean(entry.get("isDir")));
                        ValueUnit<SizeUnitEnum> filesize = UnitTk.byteToXbyte(Long.parseLong(
                                entry.get("filesize")));
                        DecimalFormat decimalFormat = new DecimalFormat("0.00");
                        DecimalFormat folderFormat = new DecimalFormat("0");

                        String filesizeStr;
                        if (filenameObj.isDirectory())
                        {
                           filesizeStr = (folderFormat.format(filesize.getValue()) + " Entries");
                           totalFolder++;
                        }
                        else
                        {
                           filesizeStr = (decimalFormat.format(filesize.getValue()) + " " +
                              filesize.getUnitString());
                           totalFiles++;
                           totalSize += Long.parseLong(entry.get("filesize"));
                        }

                        Date ctime = new Date(Long.parseLong(entry.get("ctime")));
                        Date mtime = new Date(Long.parseLong(entry.get("mtime")));
                        Date atime = new Date(Long.parseLong(entry.get("atime")));
                        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
                        row = new Object[]
                        {
                           filenameObj,
                           filesizeStr,
                           entry.get("mode"),
                           dateFormat.format(ctime),
                           dateFormat.format(mtime),
                           dateFormat.format(atime),
                           entry.get("ownerUser"),
                           entry.get("ownerGroup"),
                           filenameObj
                        };
                        model.addRow(row);
                     }
                     catch (NumberFormatException e)
                     {
                        LOGGER.log(Level.FINEST, "Number format error", e);
                     }
                  }
               }
               currentPath = pathStr;
               jTableFiles.revalidate();
            }
            else
            {
               moreToCome = false;
               jLabelInfo.setText("");
               String msg = Main.getLocal().getString("Error occured while retrieving file list!");
               LOGGER.log(Level.SEVERE, msg, true);
            }
            if (pathStr.isEmpty())
            {
               pathStr = "/";
            }
            ValueUnit<SizeUnitEnum> totalSizeVU = UnitTk.byteToXbyte(totalSize);
            DecimalFormat decimalFormat = new DecimalFormat("0.00");
            String infoText = Main.getLocal().getString("Showing ") + totalFolder + Main.getLocal().getString("folders and ")
                    + totalFiles + Main.getLocal().getString("files with a total size of ") +
                decimalFormat.format(totalSizeVU.getValue()) +
                totalSizeVU.getUnitString() + Main.getLocal().getString("in ") + pathStr;
            jLabelInfo.setText(infoText);
         }
      }
      catch (CommunicationException e)
      {
         LOGGER.log(Level.SEVERE, "Communication error during requesting URL: {0}",
            HttpTk.generateAdmonUrl("/XML_FileBrowser"));
         jLabelInfo.setText("");
      }
   }

   private static class FileBrowserTableModel extends DefaultTableModel
   {
      private static final long serialVersionUID = 1L;

      private final String[] columnNames =
      {
         Main.getLocal().getString("Name"),
         Main.getLocal().getString("Size/Entries"),
         Main.getLocal().getString("Permissions"),
         Main.getLocal().getString("Last status changed"),
         Main.getLocal().getString("Last modified"),
         Main.getLocal().getString("Last accessed"),
         Main.getLocal().getString("User"),
         Main.getLocal().getString("Group"),
         Main.getLocal().getString("Settings"),
      };

      private final Class<?>[] types =
      {
         com.beegfs.admon.gui.components.tables.FilenameObject.class,
         java.lang.String.class,
         java.lang.String.class,
         java.lang.String.class,
         java.lang.String.class,
         java.lang.String.class,
         java.lang.String.class,
         java.lang.String.class,
         com.beegfs.admon.gui.components.tables.FilenameObject.class
      };

      private final boolean[] canEdit = new boolean[]
      {
         false,
         false,
         false,
         false,
         false,
         false,
         false,
         false,
         false
      };

      @Override
      public Class<?> getColumnClass(int columnIndex)
      {
         return types[columnIndex];
      }

      @Override
      public boolean isCellEditable(int rowIndex, int columnIndex)
      {
         return canEdit[columnIndex];
      }

      @Override
      public String getColumnName(int col)
      {
         return columnNames[col];
      }

      @Override
      public int getColumnCount()
      {
         return columnNames.length;
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

      jScrollPaneFrame = new javax.swing.JScrollPane();
      jPanelFrame = new javax.swing.JPanel();
      jScrollPaneFiles = new javax.swing.JScrollPane();
      jTableFiles = new javax.swing.JTable();
      jLabelInfo = new javax.swing.JLabel();

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
      jPanelFrame.setPreferredSize(new java.awt.Dimension(917, 425));
      jPanelFrame.setLayout(new java.awt.BorderLayout(5, 5));

      jTableFiles.setAutoCreateRowSorter(true);
      jTableFiles.setModel(new FileBrowserTableModel());
      jTableFiles.setAutoResizeMode(javax.swing.JTable.AUTO_RESIZE_ALL_COLUMNS);
      jScrollPaneFiles.setViewportView(jTableFiles);

      jPanelFrame.add(jScrollPaneFiles, java.awt.BorderLayout.CENTER);

      jLabelInfo.setHorizontalAlignment(javax.swing.SwingConstants.CENTER);
      jLabelInfo.setMinimumSize(new java.awt.Dimension(100, 40));
      jLabelInfo.setPreferredSize(new java.awt.Dimension(100, 40));
      jPanelFrame.add(jLabelInfo, java.awt.BorderLayout.NORTH);

      jScrollPaneFrame.setViewportView(jPanelFrame);

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 935, Short.MAX_VALUE)
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addComponent(jScrollPaneFrame, javax.swing.GroupLayout.DEFAULT_SIZE, 451, Short.MAX_VALUE)
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void formInternalFrameClosed(javax.swing.event.InternalFrameEvent evt) {//GEN-FIRST:event_formInternalFrameClosed
       FrameManager.delFrame(this);
    }//GEN-LAST:event_formInternalFrameClosed
   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JLabel jLabelInfo;
   private javax.swing.JPanel jPanelFrame;
   private javax.swing.JScrollPane jScrollPaneFiles;
   private javax.swing.JScrollPane jScrollPaneFrame;
   private javax.swing.JTable jTableFiles;
   // End of variables declaration//GEN-END:variables

   @Override
   public final String getFrameTitle()
   {
	   return Main.getLocal().getString("File Browser");
   }
}

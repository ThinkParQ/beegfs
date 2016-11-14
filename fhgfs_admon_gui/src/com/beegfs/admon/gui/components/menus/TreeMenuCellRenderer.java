package com.beegfs.admon.gui.components.menus;

import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.components.lf.BeegfsLookAndFeel;
import java.awt.Color;
import java.awt.Component;
import java.awt.Font;
import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JTree;
import javax.swing.UIManager;
import javax.swing.tree.DefaultTreeCellRenderer;

public class TreeMenuCellRenderer extends DefaultTreeCellRenderer
{
   static final Logger LOGGER = Logger.getLogger(
      TreeMenuCellRenderer.class.getCanonicalName());
   private static final long serialVersionUID = 1L;

   private Color bgColor;
   private Font normalFont;
   private Font boldFont;

   public TreeMenuCellRenderer(Color bgColor)
   {
      super();
      this.bgColor = bgColor;
      normalFont = (Font) UIManager.get("Tree.font");
      boldFont = normalFont.deriveFont(Font.BOLD);
      ImageIcon iconImage = null;
      try
      {
         iconImage = new ImageIcon(ImageIO.read(TreeMenuCellRenderer.class.getResource(
                 FilePathsEnum.IMAGE_LEAF_ICON.getPath())));
      }
      catch (IOException e)
      {
         LOGGER.log(Level.INFO, "IO Exception while trying to load icon", e);
      }
      setLeafIcon(iconImage);
   }

   public TreeMenuCellRenderer()
   {
      this(BeegfsLookAndFeel.getMenuBackground());
   }

   @Override
   public Component getTreeCellRendererComponent(JTree tree, Object value, boolean selected,
      boolean expanded, boolean leaf, int row, boolean hasFocus)
   {
      if (leaf)
      {
         setIcon(getLeafIcon());
      }
      else if (expanded)
      {
         setIcon(getOpenIcon());
      }
      else
      {
         setIcon(getClosedIcon());
      }

      if (selected)
      {
         this.setForeground(Color.DARK_GRAY);
         this.revalidate();
      }
      else
      {
         this.setForeground(Color.BLACK);
         this.revalidate();
      }

      String input = value.toString();
      if ((input.startsWith("_")) && (input.endsWith("_")))
      {
         setText(input.substring(1, input.length() - 2));
         setEnabled(false);
         setIcon(getDisabledIcon());
      }
      else
      {
         setText(input);
      }
      return this;
   }
}

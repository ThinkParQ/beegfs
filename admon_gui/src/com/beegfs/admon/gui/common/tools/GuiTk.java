package com.beegfs.admon.gui.common.tools;

import com.beegfs.admon.gui.common.enums.FilePathsEnum;
import com.beegfs.admon.gui.components.charts.ChartPoint;
import com.beegfs.admon.gui.components.charts.LineChart;
import java.awt.Color;
import java.awt.Image;
import static java.awt.Image.SCALE_AREA_AVERAGING;
import java.io.IOException;
import java.util.ArrayList;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.imageio.ImageIO;
import javax.swing.ImageIcon;


public class GuiTk
{
   static final Logger LOGGER = Logger.getLogger(GuiTk.class.getCanonicalName());

   public static Color getFhGColor()
   {
      return new Color(0,238,102);
   }

   public static Color getGreen()
   {
      return new Color(0,140,50);
   }

   public static Color getRed()
   {
      return new Color(180,0,30);
   }

   public static Color getTurqouise()
   {
      return new Color(0,255,204);
   }

   public static Color getPanelGrey()
   {
      return new Color(230,230,230);
   }

   public static ImageIcon getFrameIcon()
   {
      ImageIcon iconImage = null;
      try
      {
         iconImage = new ImageIcon(ImageIO.read(GuiTk.class.getResource(
            FilePathsEnum.IMAGE_BEEGFS_ICON.getPath())));
      }
      catch (IOException e)
      {
         LOGGER.log(Level.INFO, "IO Exception while trying to load icon.", e);
      }
      return iconImage;
   }

   public static ImageIcon resizeImage(ImageIcon unscaledImage, int height, int widht)
   {
      Image img = unscaledImage.getImage();
      Image newImg = img.getScaledInstance(widht, height, SCALE_AREA_AVERAGING);
      return new ImageIcon(newImg);
   }

   @SuppressWarnings("AssignmentToMethodParameter")
   public static void addTracePointsToChart(ArrayList<ChartPoint> tracePoints, LineChart chart,
      String traceName, long timeSpan, boolean isAverage, Color color,
      ArrayList<Integer> valueCountOfOtherTraces)
   {
      // minimal 2 values are needed for printing a trace
      if (tracePoints.size() < 2)
      {
         if (isAverage)
         {
            chart.disableTrace(traceName);
            chart.disableLegend(traceName);

            return;
         }

         int count = calculateTracePointCount(timeSpan, valueCountOfOtherTraces);
         tracePoints = ChartPoint.generateChartPoints(count, 0.0, timeSpan);
         chart.setTracePoints(traceName, tracePoints, color);
      }
      else
      {
         chart.setTracePoints(traceName, tracePoints, color);

         if (isAverage)
         {
            if (chart.isCheckboxSelected(traceName))
            {
               chart.enableTrace(traceName);
            }
            chart.enableLegend(traceName);
         }
      }
   }

   private static int calculateTracePointCount(long timeSpan,
      ArrayList<Integer> valueCountOfOtherTraces)
   {
      int count = 0;

      // calculate TracePoint count from other traces
      for (int countTracePoints : valueCountOfOtherTraces)
      {
         if (countTracePoints > count)
         {
            count = countTracePoints;
         }
      }

      // minimal 2 values are needed for printing a trace
      if (count > 2)
      {
         return count;
      }

      // calculate TracePoint count from timespan
      if (timeSpan > 7200)
      {
         count = (int) timeSpan / 1440;
      }
      else if (timeSpan > 1440)
      {
         count = (int) timeSpan / 220;
      }
      else
      {
         count = 10;
      }

      return count;
   }

   private GuiTk()
   {
   }
}

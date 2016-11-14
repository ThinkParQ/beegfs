package com.beegfs.admon.gui.components.charts;


import com.beegfs.admon.gui.common.enums.ColorLineTraceEnum;
import info.monitorenter.gui.chart.IAxis;
import info.monitorenter.gui.chart.ITrace2D;
import info.monitorenter.gui.chart.ZoomableChart;
import info.monitorenter.gui.chart.labelformatters.LabelFormatterDate;
import info.monitorenter.gui.chart.labelformatters.LabelFormatterNumber;
import info.monitorenter.gui.chart.traces.Trace2DSorted;
import info.monitorenter.gui.chart.traces.painters.TracePainterFill;
import info.monitorenter.gui.chart.traces.painters.TracePainterLine;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Component;
import java.awt.Font;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.text.DateFormat;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.SortedSet;
import javax.swing.JLabel;
import javax.swing.JPanel;



public class LineChart extends JPanel
{
   private static final long serialVersionUID = 1L;

   private ZoomableChart chart;
   private JPanel panelLegend;

   public LineChart(String title, String xAxis, String yAxis, int maxTraceSize,
           LineTrace[] traces)
   {
      this(title, xAxis, yAxis, maxTraceSize, traces, false, true);
   }

   public LineChart(String title, String xAxis, String yAxis, int maxTraceSize,
           LineTrace[] traces, boolean fill, boolean toggable)
   {
      this(title, xAxis, yAxis, maxTraceSize, traces, fill, toggable, false);
   }

   public LineChart(String title, String xAxis, String yAxis, int maxTraceSize,
           LineTrace[] traces, boolean fill, boolean toggable, boolean fullDateTime)
   {
      this.setVisible(true);

      BorderLayout layout = new BorderLayout();

      this.setLayout(layout);
      this.setDoubleBuffered(true);

      JLabel titleLabel = new JLabel(title, JLabel.CENTER);
      titleLabel.setFont(new Font("Serif", Font.BOLD, 30));

      titleLabel.setFont(titleLabel.getFont().deriveFont(80));
      titleLabel.setVisible(true);
      this.add(titleLabel, BorderLayout.NORTH);

      chart = new ZoomableChart();
      chart.setVisible(true);
      chart.addMouseListener(new LineChartMouseListener() );

      IAxis axisX = chart.getAxisX();
      axisX.getAxisTitle().setTitle(xAxis);
      
      if (fullDateTime)
      {
         axisX.setFormatter(new LabelFormatterDate(
                 (SimpleDateFormat) DateFormat.getDateTimeInstance(DateFormat.SHORT,
                 DateFormat.SHORT)));
      }
      else
      {
         axisX.setFormatter(new LabelFormatterDate(
                 (SimpleDateFormat) DateFormat.getTimeInstance(DateFormat.SHORT)));
      }
      axisX.setPaintGrid(true);

      IAxis axisY = chart.getAxisY();
      axisY.getAxisTitle().setTitle(yAxis);
      axisY.setFormatter(new LabelFormatterNumber(new DecimalFormat("##")));
      axisY.setPaintGrid(true);

      chart.setPaintLabels(false);
      chart.setGridColor(ColorLineTraceEnum.COLOR_GRID.color());

      panelLegend = new JPanel();

      for (LineTrace trace : traces)
      {
         Trace2DSorted t = new Trace2DSorted();

         chart.addTrace(t);

         t.setName(trace.getName());
         t.setColor(trace.getColor());

         if (fill)
         {
            t.addTracePainter(new TracePainterFill(chart));
         }
         else
         {
            t.addTracePainter(new TracePainterLine());
         }

         if (toggable)
         {
            panelLegend.add(new LegendItemToggable(this, trace.getColor(), trace.getName()));
            selectCheckbox(trace.getName());
         }
         else
         {
            panelLegend.add(new LegendItemNotToggable(this, trace.getColor(), trace.getName()));
         }
         
         for (ChartPoint p : trace.getValues())
         {
            t.addPoint(p.getTime(), p.getValue());
         }
      }

      this.add(chart, BorderLayout.CENTER);

      panelLegend.setVisible(true);

      this.add(panelLegend, BorderLayout.PAGE_END);
   }

   final public void disableLegend(String name)
   {
      Component[] componentList = panelLegend.getComponents();
      
      for (Component componentList1 : componentList)
      {
         LegendItemToggable item = (LegendItemToggable) componentList1;
         if (item.getName().equals(name))
         {
            item.setEnabled(false);
         }
      }
   }

   final public void enableLegend(String name)
   {
      Component[] componentList = panelLegend.getComponents();
      
      for (Component componentList1 : componentList)
      {
         LegendItemToggable item = (LegendItemToggable) componentList1;
         if (item.getName().equals(name))
         {
            item.setEnabled(true);
         }
      }
   }

   final public void deselectCheckbox(String name)
   {
      Component[] componentList = panelLegend.getComponents();
      
      for (Component componentList1 : componentList)
      {
         LegendItemToggable item = (LegendItemToggable) componentList1;
         if (item.getName().equals(name))
         {
            item.deselectCheckbox();
         }
      }
   }

   final public void selectCheckbox(String name)
   {
      Component[] componentList = panelLegend.getComponents();
      
      for (Component componentList1 : componentList)
      {
         LegendItemToggable item = (LegendItemToggable) componentList1;
         if (item.getName().equals(name))
         {
            item.selectCheckbox();
         }
      }
   }

   final public boolean isCheckboxSelected(String name)
   {
      Component[] componentList = panelLegend.getComponents();
      
      for (Component componentList1 : componentList)
      {
         LegendItemToggable item = (LegendItemToggable) componentList1;
         if (item.getName().equals(name))
         {
            return item.isCheckboxSelected();
         }
      }

      return false;
   }

   public void disableTrace(String name)
   {
      SortedSet<ITrace2D> traces = chart.getTraces();
      Iterator<ITrace2D> iter = traces.iterator();
      
      while (iter.hasNext())
      {
         Trace2DSorted t = (Trace2DSorted) iter.next();
         
         if (t.getName().equals(name))
         {
            t.setVisible(false);
            break;
         }
      }
   }

   public void enableTrace(String name)
   {
      SortedSet<ITrace2D> traces = chart.getTraces();
      Iterator<ITrace2D> iter = traces.iterator();
      
      while (iter.hasNext())
      {
         Trace2DSorted t = (Trace2DSorted) iter.next();
         
         if (t.getName().equals(name))
         {
            t.setVisible(true);
            break;
         }
      }
   }

   public void addTracePoints(String name, ArrayList<ChartPoint> tracepoints)
   {
      SortedSet<ITrace2D> traces = chart.getTraces();
      Iterator<ITrace2D> iter = traces.iterator();
      
      while (iter.hasNext())
      {
         Trace2DSorted t = (Trace2DSorted) iter.next();
         
         if (t.getName().equals(name))
         {
            for (ChartPoint p : tracepoints)
            {
               t.addPoint(p.getTime(), p.getValue());
            }
            break;
         }

      }
   }

   public void setTracePoints(String name, ArrayList<ChartPoint> tracepoints, Color traceColor)
   {
      SortedSet<ITrace2D> traces = chart.getTraces();
      Iterator<ITrace2D> iter = traces.iterator();
      
      while (iter.hasNext())
      {
         Trace2DSorted t = (Trace2DSorted) iter.next();
         
         if (t.getName().equals(name))
         {
            t.removeAllPoints();
            t.setColor(traceColor);

            for (ChartPoint p : tracepoints)
            {
               t.addPoint(p.getTime(), p.getValue());
            }
            break;
         }
      }
   }

   public void addTracePoints(LineTrace trace)
   {
      addTracePoints(trace.getName(), trace.getValues());
   }

   public void setFullDateFormat(boolean fullDateFormat)
   {
      if (fullDateFormat)
      {
         chart.getAxisX().setFormatter(new LabelFormatterDate(
                 (SimpleDateFormat) DateFormat.getDateTimeInstance(DateFormat.SHORT,
                 DateFormat.SHORT)));
      }
      else
      {
         chart.getAxisX().setFormatter(new LabelFormatterDate(
                 (SimpleDateFormat) DateFormat.getTimeInstance(DateFormat.SHORT)));
      }
      chart.revalidate();
   }

   private class LineChartMouseListener implements MouseListener
   {
      @Override
      public void mouseClicked(MouseEvent e)
      {
      }

      @Override
      public void mousePressed(MouseEvent e)
      {
         if (e.isPopupTrigger())
         {
            chart.zoomAll();
         }
      }

      @Override
      public void mouseReleased(MouseEvent e)
      {
      }

      @Override
      public void mouseEntered(MouseEvent e)
      {
      }

      @Override
      public void mouseExited(MouseEvent e)
      {
      }
   }
}

package com.beegfs.admon.gui.components.charts;


import java.awt.Color;
import java.util.ArrayList;


public class LineTrace
{
   private final static int INITIAL_CAPACITY = 50;
   private final String name;
   private final ArrayList<ChartPoint> values;
   private final Color color;

   public LineTrace()
   {
      name = "";
      color = Color.BLACK;
      values = new ArrayList<>(INITIAL_CAPACITY);
   }

   public LineTrace(String name, Color color, ArrayList<ChartPoint> values)
   {
      this.name = name;
      this.color = color;
      this.values = values;
   }

   public String getName()
   {
      return this.name;
   }

   public Color getColor()
   {
      return this.color;
   }

   @SuppressWarnings("ReturnOfCollectionOrArrayField")
   public ArrayList<ChartPoint> getValues()
   {
      return this.values;
   }
}

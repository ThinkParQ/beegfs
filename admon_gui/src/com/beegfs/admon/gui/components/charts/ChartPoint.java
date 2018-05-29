package com.beegfs.admon.gui.components.charts;

import java.util.ArrayList;


public class ChartPoint {

   public static ArrayList<ChartPoint> generateChartPoints(int count, double value, long timeStart,
      long timeEnd)
   {
      ArrayList<ChartPoint> retVal = new ArrayList<>(count);

      if (count == 1)
      {
         return retVal;
      }

      if (timeStart >= timeEnd)
      {
         return retVal;
      }

      for(int index = 0; index < count; index++)
      {
         double time = (((timeEnd - timeStart) / (count -1 )) * index) + timeStart;

         retVal.add(new ChartPoint(time, value));
      }

      return retVal;
   }

   public static ArrayList<ChartPoint> generateChartPoints(int count, double value, long timespanMin)
   {
      long now = System.currentTimeMillis();
      long past = (now - (timespanMin * 60000));

      return generateChartPoints(count, value, past, now);
   }

   private final double time;
   private final double value;

    public ChartPoint()
    {
        this.time = 0.0;
        this.value = 0.0;
    }

    public ChartPoint(double time, double value)
    {
        this.time = time;
        this.value = value;
   }

   public double getTime()
   {
      return this.time;
   }

   public double getValue()
   {
      return this.value;
   }

}

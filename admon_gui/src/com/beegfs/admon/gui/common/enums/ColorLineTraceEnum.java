package com.beegfs.admon.gui.common.enums;


import java.awt.Color;


public enum ColorLineTraceEnum
{
   COLOR_AVERAGE_READ(new Color(30, 144, 250)),
   COLOR_AVERAGE_WRITE(Color.RED),
   COLOR_READ(new Color(0, 0, 128)),
   COLOR_WRITE(new Color(178, 34, 34)),
   COLOR_META_OPS(Color.RED),
   COLOR_GRID(Color.LIGHT_GRAY);

   private final Color color;

   ColorLineTraceEnum(Color color)
   {
      this.color = color;
   }

   public Color color()
   {
      return color;
   }
}

package com.beegfs.admon.gui.common.enums;


import java.awt.Color;


/**
 * Keep the enum in sync with the common/nodes/Node.h
 */
public enum NodeTypesEnum
{
   INVALID("invalid", "invalid", new Color(Color.GRAY.getRed(), Color.GRAY.getGreen(),
      Color.GRAY.getBlue(), NodeTypesEnum.alpha)),
   METADATA("metadata", "meta", new Color(Color.BLUE.getRed(), Color.BLUE.getGreen(),
      Color.BLUE.getBlue(), NodeTypesEnum.alpha)),
   STORAGE("storage", "storage", new Color(Color.GREEN.getRed(), Color.GREEN.getGreen(),
      Color.GREEN.getBlue(), NodeTypesEnum.alpha)),
   CLIENT("client", "client", new Color(Color.YELLOW.getRed(), Color.YELLOW.getGreen(),
      Color.YELLOW.getBlue(), NodeTypesEnum.alpha)),
   MANAGMENT("managment", "mgmtd", new Color(Color.RED.getRed(), Color.RED.getGreen(),
      Color.RED.getBlue(), NodeTypesEnum.alpha)),
   HELPERD("helperd", "helperd", new Color(Color.GRAY.getRed(), Color.GRAY.getGreen(),
      Color.GRAY.getBlue(), NodeTypesEnum.alpha)),
   ADMON("admon", "admon", new Color(Color.GRAY.getRed(), Color.GRAY.getGreen(),
      Color.GRAY.getBlue(), NodeTypesEnum.alpha));

   private static final int alpha = 50;

   private final String type;
   private final String shortType;
   private final Color color;

   NodeTypesEnum(String type, String shortType, Color color)
   {
      this.type = type;
      this.shortType = shortType;
      this.color = color;
   }

   public String type()
   {
      return type;
   }

   public String shortType()
   {
      return shortType;
   }

   public Color color()
   {
      return color;
   }
}

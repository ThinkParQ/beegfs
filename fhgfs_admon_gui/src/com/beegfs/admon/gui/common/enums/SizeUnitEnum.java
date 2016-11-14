package com.beegfs.admon.gui.common.enums;



public enum SizeUnitEnum implements UnitEnum
{
   NONE("",""),
   BYTE("Byte", "byte"),
   KIBIBYTE("KiB", "kibibyte"),
   MEBIBYTE("MiB", "mebibyte"),
   GIBIBYTE("GiB", "gibibyte"),
   TEBIBYTE("TiB", "tebibyte"),
   PEBIBYTE("PiB", "pebibyte"),
   EXBIBYTE("EiB", "exbibyte"),
   ZEBIBYTE("ZiB", "zebibyte"),
   YOBIBYTE("YiB", "yobibyte");

   private final String unit;
   private final String description;

   SizeUnitEnum(String unit, String description)
   {
      this.unit = unit;
      this.description = description;
   }

   @Override
   public String getUnit()
   {
      return this.unit;
   }

   @Override
   public String getDescription()
   {
      return this.description;
   }

   @Override
   public int getID()
   {
      return this.ordinal();
   }
}

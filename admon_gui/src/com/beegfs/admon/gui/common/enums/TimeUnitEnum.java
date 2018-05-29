package com.beegfs.admon.gui.common.enums;


public enum TimeUnitEnum implements UnitEnum
{
   NONE("",""),
   MILLISECONDS("ms", "milliseconds"),
   SECONDS("s", "seconds"),
   MINUTES("m", "minutes"),
   HOURS("h", "hours"),
   DAYS("d", "days");

   private final String unit;
   private final String description;

   TimeUnitEnum(String unit, String description)
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

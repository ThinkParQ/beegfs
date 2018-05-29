package com.beegfs.admon.gui.common;

import com.beegfs.admon.gui.common.enums.SizeUnitEnum;
import com.beegfs.admon.gui.common.enums.TimeUnitEnum;
import com.beegfs.admon.gui.common.enums.UnitEnum;
import java.text.DecimalFormat;
import java.util.Objects;

public class ValueUnit<T extends UnitEnum> implements Comparable<ValueUnit<T>>
{
   private double value;
   private T unit;

   public ValueUnit(double newValue, T newUnit)
   {
      value = newValue;
      unit = newUnit;
   }

   @Override
   public String toString()
   {
      DecimalFormat df = new DecimalFormat("0.00");
      return df.format(this.value) + " " + this.unit.getUnit();
   }

   public T getUnit()
   {
      return this.unit;
   }

   public String getUnitString()
   {
      return this.unit.getUnit();
   }

   public double getValue()
   {
      return this.value;
   }

   public void setUnit(T unit)
   {
      this.unit = unit;
   }

   public void setValue(double value)
   {
      this.value = value;
   }

   public void update(double newValue, T newUnit)
   {
      value = newValue;
      unit = newUnit;
   }

   @Override
   public int compareTo(ValueUnit<T> o)
   {
      int retVal;
      if (this.unit.getID() == o.getUnit().getID() )
      {
            retVal = Double.compare(this.value, o.getValue() );
      }
      else if (this.unit.getID() > o.getUnit().getID() )
      {
         retVal = 1;
      }
      else
      {
         retVal = -1;
      }

      return retVal;
   }

   @Override
   public boolean equals(Object o)
   {
      boolean retVal = false;
      if (o instanceof ValueUnit)
      {
         if( (this.unit == ((ValueUnit)o).getUnit() ) &&
             Double.compare(this.value, ((ValueUnit)o).getValue() ) == 0 )
         {
            return true;
         }
      }

      return retVal;
   }

   @Override
   public int hashCode()
   {
      int hash = 5;
      hash = 37 * hash +
         (int) (Double.doubleToLongBits(this.value) ^
         (Double.doubleToLongBits(this.value) >>> 32));
      hash = 37 * hash + Objects.hashCode(this.unit);
      return hash;
   }

   public boolean isValid()
   {
      if (this.unit instanceof SizeUnitEnum)
      {
         SizeUnitEnum thisUnit = (SizeUnitEnum) this.unit;
         return thisUnit != SizeUnitEnum.NONE;
      }
      else if (this.unit instanceof TimeUnitEnum)
      {
         TimeUnitEnum thisUnit = (TimeUnitEnum) this.unit;
         return thisUnit != TimeUnitEnum.NONE;
      }

      return false;
   }
}
